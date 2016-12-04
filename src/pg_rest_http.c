/* -------------------------------------------------------------------------
 *
 * pq_rest_http.c
 *   http common routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

#include "pg_rest_http_postgres.h"
#include "pg_rest_http_push_stream.h"

static pgrest_array_t *pgrest_http_ports = NULL;

static pgrest_listener_t *
pgrest_http_add_listener(pgrest_http_conf_addr_t *addr)
{
    pgrest_listener_t         *listener;
    pgrest_conf_http_server_t *conf_server;

    listener = pgrest_listener_create(&addr->opt.sockaddr.sockaddr,
                                      addr->opt.socklen);
    listener->addr_ntop = 1;

    listener->handler = pgrest_http_conn_init;

    conf_server = addr->default_server;
    listener->post_accept_timeout = conf_server->client_header_timeout;

    listener->backlog = addr->opt.backlog;
    listener->rcvbuf = addr->opt.rcvbuf;
    listener->sndbuf = addr->opt.sndbuf;

    listener->keepalive = addr->opt.so_keepalive;

#ifdef HAVE_KEEPALIVE_TUNABLE
    listener->keepidle = addr->opt.tcp_keepidle;
    listener->keepintvl = addr->opt.tcp_keepintvl;
    listener->keepcnt = addr->opt.tcp_keepcnt;
#endif
#if defined(HAVE_DEFERRED_ACCEPT) && defined(SO_ACCEPTFILTER)
    listener->accept_filter = pstrdup(addr->opt.accept_filter);
    listener->add_deferred = strlen(listener->accept_filter) ? 1 : 0;
#endif
#if defined(HAVE_DEFERRED_ACCEPT) && defined(TCP_DEFER_ACCEPT)
    listener->deferred_accept = addr->opt.deferred_accept;
    listener->add_deferred = listener->deferred_accept ? 1 : 0;
#endif
#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
    listener->ipv6only = addr->opt.ipv6only;
#endif
#ifdef HAVE_SETFIB
    listener->setfib = addr->opt.setfib;
#endif
#ifdef HAVE_TCP_FASTOPEN
    listener->fastopen = addr->opt.fastopen;
#endif
#ifdef HAVE_REUSEPORT
    listener->reuseport = addr->opt.reuseport;
#endif
#ifdef HAVE_UNIX_SOCKETS
    if (listener->sockaddr->sa_family == AF_UNIX) {
        listener->reuseport = 0;
    }
#endif
    listener->open = 1;

    return listener;
}

static bool
pgrest_http_add_server(pgrest_conf_http_server_t *conf_server,
                       pgrest_http_conf_addr_t *addr)
{
    pgrest_uint_t               i;
    pgrest_conf_http_server_t **server;

    if (addr->servers.elts == NULL) {
        if (pgrest_array_init(&addr->servers, 
                              CurrentMemoryContext, 
                              2, 
                              sizeof(pgrest_conf_http_server_t *)) == false) 
        {
            return false;
        }
    } else {
        server = addr->servers.elts;
        for (i = 0; i < addr->servers.size; i++) {
            if (server[i] == conf_server) {
                ereport(WARNING, (errmsg(PGREST_PACKAGE " " "a duplicate "
                                         "listen %s", addr->opt.addr)));
                return false;
            }
        }
    }

    server = pgrest_array_push(&addr->servers);
    if (server == NULL) {
        return false;
    }

    *server = conf_server;

    return true;
}

static bool 
pgrest_http_add_address(pgrest_conf_http_server_t *conf_server,
                        pgrest_http_conf_port_t *port, 
                        pgrest_conf_listener_t *conf_listener)
{
    pgrest_http_conf_addr_t   *addr;
    HASHCTL                    hash_ctl;
    pgrest_http_server_name_t *sn;

    if (port->addrs.elts == NULL) {
        if (pgrest_array_init(&port->addrs, 
                              CurrentMemoryContext, 
                              2, 
                              sizeof(pgrest_http_conf_addr_t)) == false) 
        {
            return false;
        }
    }

    /* TODO  */
#if defined(HAVE_OPENSSL)                  \
    && !defined(HAVE_TLSEXT_TYPE_ALPN)     \
    && !defined(HAVE_TLSEXT_TYPE_NPN)

    if (conf_listener->http2 && conf_listener->ssl) {
        ereport(WARNING, 
                (errmsg(PGREST_PACKAGE " " "was built with OpenSSL that lacks "
                        "ALPN and NPN support, HTTP/2 is not enabled for %s", 
                         conf_listener->addr)));
    }

#endif

    addr = pgrest_array_push(&port->addrs);
    if (addr == NULL) {
        return false;
    }

    addr->opt = *conf_listener;
    addr->default_server = conf_server;
    addr->servers.elts = NULL;

    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(sn->name);
    hash_ctl.entrysize = sizeof(pgrest_http_server_name_t);

    addr->hash = hash_create("Server Names", 8, &hash_ctl, HASH_ELEM);

    return pgrest_http_add_server(conf_server, addr);
}

static bool
pgrest_http_add_addresses(pgrest_conf_http_server_t *conf_server,
                          pgrest_http_conf_port_t *port, 
                          pgrest_conf_listener_t *conf_listener)

{
    pgrest_uint_t             i;
    pgrest_uint_t             default_server;
    pgrest_http_conf_addr_t  *addr;
#ifdef HAVE_OPENSSL 
    pgrest_uint_t             ssl;
#endif
    pgrest_uint_t             http2;

    addr = port->addrs.elts;

    for (i = 0; i < port->addrs.size; i++) {

        if (!pgrest_inet_cmp_sockaddr(&conf_listener->sockaddr.sockaddr, 
                                      conf_listener->socklen,
                                      &addr[i].opt.sockaddr.sockaddr,
                                      addr[i].opt.socklen, 0)
            )
        {
            continue;
        }

        /* the address is already in the address list */
        if (!pgrest_http_add_server(conf_server, &addr[i])) {
            return false;
        }

        /* preserve default_server bit during listen options overwriting */
        default_server = addr[i].opt.default_server;

#ifdef HAVE_OPENSSL
        ssl = conf_listener->ssl || addr[i].opt.ssl;
#endif
        http2 = conf_listener->http2 || addr[i].opt.http2;

        if (conf_listener->set) {

            if (addr[i].opt.set) {
                ereport(WARNING, (errmsg(PGREST_PACKAGE " " "duplicate listen "
                                         "options for %s", addr->opt.addr)));
                return false;
            }

            addr[i].opt = *conf_listener;
        }

        /* check the duplicate "default" server for this address:port */
        if (conf_listener->default_server) {

            if (default_server) {
                ereport(WARNING, (errmsg(PGREST_PACKAGE " " "a duplicate default "
                                         "server for %s", addr->opt.addr)));
                return false;
            }

            default_server = 1;
            addr[i].default_server = conf_server;
        }

        addr[i].opt.default_server = default_server;
#ifdef HAVE_OPENSSL
        addr[i].opt.ssl = ssl;
#endif
        addr[i].opt.http2 = http2;

        return true;
    }

    /* add the address to the addresses list that bound to this port */
    return pgrest_http_add_address(conf_server, port, conf_listener);
}

static bool
pgrest_http_add_listen(pgrest_conf_http_server_t *conf_server,
                       pgrest_conf_listener_t *conf_listener,
                       pgrest_array_t *ports)
{
    in_port_t                   port_number;
    pgrest_uint_t               i;
    struct sockaddr            *sa;
    pgrest_http_conf_port_t    *port;

    sa = &conf_listener->sockaddr.sockaddr;
    port_number = pgrest_inet_get_port(sa);

    port = ports->elts;
    for (i = 0; i < ports->size; i++) {

        if (port_number != port[i].port || sa->sa_family != port[i].family) {
            continue;
        }

        /* a port is already in the port list */
        return pgrest_http_add_addresses(conf_server, &port[i], conf_listener);
    }

    port = pgrest_array_push(ports);
    if (port == NULL) {
        return false;
    }

    port->family = sa->sa_family;
    port->port = port_number;
    port->addrs.elts = NULL;

    return pgrest_http_add_address(conf_server, port, conf_listener);
}

static bool
pgrest_http_server_names(pgrest_http_conf_addr_t *addr)
{
    pgrest_uint_t               n, s;
    pgrest_http_server_name_t  *name;
    pgrest_conf_http_server_t **conf_server;
    pgrest_http_server_name_t  *entry;
    bool                        found;

    conf_server = addr->servers.elts;

    for (s = 0; s < addr->servers.size; s++) {

        name = conf_server[s]->server_names.elts;

        for (n = 0; n < conf_server[s]->server_names.size; n++) {
            /* Add entry to hash table */
            entry = (pgrest_http_server_name_t* ) hash_search(addr->hash,
                                                              name[n].name,
                                                              HASH_ENTER,
                                                              &found);
            /* Shouldn't get a duplicate entry */
            if (found) {
                ereport(WARNING, (errmsg(PGREST_PACKAGE " " "conflicting "
                                         "server name \"%s\" on %s", 
                                          name[n].name , addr->opt.addr)));
            }

            /* Fill in the hash table entry */
            entry->server = name[n].server;
        }
    }

    return true;
}

static int
pgrest_http_cmp_conf_addrs(const void *one, const void *two)
{
    pgrest_http_conf_addr_t  *first, *second;

    first = (pgrest_http_conf_addr_t *) one;
    second = (pgrest_http_conf_addr_t *) two;

    if (first->opt.wildcard) {
        /* a wildcard address must be the last resort, shift it to the end */
        return 1;
    }

    if (second->opt.wildcard) {
        /* a wildcard address must be the last resort, shift it to the end */
        return -1;
    }

    if (first->opt.bind && !second->opt.bind) {
        /* shift explicit bind()ed addresses to the start */
        return -1;
    }

    if (!first->opt.bind && second->opt.bind) {
        /* shift explicit bind()ed addresses to the start */
        return 1;
    }

    /* do not sort by default */
    return 0;
}

static bool
pgrest_http_add_addrs(pgrest_http_port_t *hport, pgrest_http_conf_addr_t *addr)
{
    pgrest_uint_t                 i;
    pgrest_http_in_addr_t        *addrs;
    struct sockaddr_in           *sin;
    pgrest_http_virtual_names_t  *vn;

    hport->addrs = palloc0(hport->naddrs * sizeof(pgrest_http_in_addr_t));
    addrs = hport->addrs;

    for (i = 0; i < hport->naddrs; i++) {

        sin = &addr[i].opt.sockaddr.sockaddr_in;
        addrs[i].addr = sin->sin_addr.s_addr;
        addrs[i].conf.default_server = addr[i].default_server;
#ifdef HAVE_OPENSSL
        addrs[i].conf.ssl = addr[i].opt.ssl;
#endif
        addrs[i].conf.http2 = addr[i].opt.http2;

        vn = palloc(sizeof(pgrest_http_virtual_names_t));
        vn->hash = addr[i].hash;

        addrs[i].conf.virtual_names = vn;
    }

    return true;
}

#ifdef HAVE_IPV6
static bool
pgrest_http_add_addrs6(pgrest_http_port_t *hport, pgrest_http_conf_addr_t *addr)
{
    pgrest_uint_t                 i;
    pgrest_http_in6_addr_t       *addrs6;
    struct sockaddr_in6          *sin6;
    pgrest_http_virtual_names_t  *vn;

    hport->addrs = palloc0(hport->naddrs * sizeof(pgrest_http_in6_addr_t));
    addrs6 = hport->addrs;

    for (i = 0; i < hport->naddrs; i++) {

        sin6 = &addr[i].opt.sockaddr.sockaddr_in6;
        addrs6[i].addr6 = sin6->sin6_addr;
        addrs6[i].conf.default_server = addr[i].default_server;
#ifdef HAVE_OPENSSL
        addrs6[i].conf.ssl = addr[i].opt.ssl;
#endif
        addrs6[i].conf.http2 = addr[i].opt.http2;

        vn = palloc(sizeof(pgrest_http_virtual_names_t));

        addrs6[i].conf.virtual_names = vn;

        vn->hash = addr[i].hash;
    }

    return true;
}
#endif

static bool
pgrest_http_init_listener(pgrest_http_conf_port_t *port)
{
    pgrest_uint_t               i, last, bind_wildcard;
    pgrest_listener_t          *listener;
    pgrest_http_port_t         *hport;
    pgrest_http_conf_addr_t    *addr;

    addr = port->addrs.elts;
    last = port->addrs.size;

    if (addr[last - 1].opt.wildcard) {
        addr[last - 1].opt.bind = 1;
        bind_wildcard = 1;

    } else {
        bind_wildcard = 0;
    }

    i = 0;

    while (i < last) {

        if (bind_wildcard && !addr[i].opt.bind) {
            i++;
            continue;
        }

        listener = pgrest_http_add_listener(&addr[i]);
        hport = palloc0(sizeof(pgrest_http_port_t));

        listener->servers = hport;
        hport->naddrs = i + 1;

        switch (listener->sockaddr->sa_family) {

#ifdef HAVE_IPV6
        case AF_INET6:
            if (!pgrest_http_add_addrs6(hport, addr)) {
                return false;
            }
            break;
#endif
        default: /* AF_INET */
            if (!pgrest_http_add_addrs(hport, addr)) {
                return false;
            }
            break;
        }

        addr++;
        last--;
    }

    return true;
}

static bool 
pgrest_http_optimize_servers(pgrest_array_t *ports)
{
    pgrest_uint_t             p;
    pgrest_uint_t             a;
    pgrest_http_conf_port_t  *port;
    pgrest_http_conf_addr_t  *addr;

    if (ports == NULL) {
        return true;
    }

    port = ports->elts;
    for (p = 0; p < ports->size; p++) {

        pgrest_util_sort(port[p].addrs.elts, 
                         (size_t) port[p].addrs.size,
                         sizeof(pgrest_http_conf_addr_t), 
                         pgrest_http_cmp_conf_addrs);

        /*
         * check whether all name-based servers have the same
         * configuration as a default server for given address:port
         */
        addr = port[p].addrs.elts;
        for (a = 0; a < port[p].addrs.size; a++) {

            if (addr[a].servers.size > 1 ) {
                if (!pgrest_http_server_names(&addr[a])) {
                    return false;
                }
            }
        }

        if (!pgrest_http_init_listener(&port[p])) {
            return false;
        }
    }

    return true;
}

static void
pgrest_http_pathtree_waler(pgrest_conf_http_path_t *path)
{
    int                       i;
    pgrest_http_handler_t   **handler;

    if (path) {
        handler = path->handlers.elts;
        for (i = 0; i < path->handlers.size; i++) {
            if (handler[i]->init(handler[i]) == false) {
                ereport(ERROR, (errmsg(PGREST_PACKAGE " " "initialize http "
                                       "\"%s\" failed", handler[i]->name)));
            }
        }
    }
}

void 
pgrest_http_setup_configurators(void)
{
    /* http handler&filter configure handler*/
    pgrest_http_upstream_conf_register();
    pgrest_http_postgres_conf_register();
    pgrest_http_push_stream_conf_register();
}

void 
pgrest_http_init(pgrest_setting_t *setting)
{
    pgrest_uint_t              i;
    pgrest_conf_http_server_t *conf_server;
    pgrest_conf_listener_t    *conf_listener;

    if (!pgrest_http_upstream_init()) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "initialize http upstream failed")));
    }

    conf_server = setting->conf_http_servers.elts;
    for (i = 0; i < setting->conf_http_servers.size; i++) {
        pgrest_rtree_walker(conf_server[i].paths, pgrest_http_pathtree_waler);
    }

    pgrest_http_ports = pgrest_array_create(CurrentMemoryContext, 
                                            2, 
                                            sizeof(pgrest_http_conf_port_t));
    if (pgrest_http_ports == NULL) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "out of memory")));
    }

    conf_listener = setting->conf_listeners.elts;
    for (i = 0; i < setting->conf_listeners.size; i++) {
        if(!pgrest_http_add_listen(&conf_server[i], 
                                   &conf_listener[i],
                                   pgrest_http_ports)) {
            ereport(ERROR,
                    (errmsg(PGREST_PACKAGE " " "http add listener failed")));
        }
    }

    if (!pgrest_http_optimize_servers(pgrest_http_ports)) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "http optimize servers failed")));
    }
}
