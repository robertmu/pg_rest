/* -------------------------------------------------------------------------
 *
 * pq_rest_listener.c
 *   listener routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_listener.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct {
    pgrest_listener_hook_pt  init;
    void                    *data;
} pgrest_listener_hook_t;

static List *pgrest_listener_hooks = NIL;
static List *pgrest_listener_listeners = NIL;

static pgrest_listener_t *
pgrest_listener_create(void *sockaddr, socklen_t socklen,
                        pgrest_conn_handler_pt conn_handler)
{
    pgrest_listener_t *listener;
    struct sockaddr   *sa;
    char               text[MAXPGPATH] = {0};

    listener = (pgrest_listener_t *) palloc0fast(sizeof(pgrest_listener_t));

    sa = (struct sockaddr *) palloc(socklen);
    memcpy(sa, sockaddr, socklen);

    listener->sockaddr = sa;
    listener->socklen = socklen;

    (void) pgrest_util_inet_ntop(sa, socklen, text, MAXPGPATH);
    listener->addr_text = pstrdup(text);

    listener->fd = (evutil_socket_t) -1;
    listener->type = SOCK_STREAM;

    listener->addr_ntop = 1;
    listener->handler = conn_handler;

    listener->backlog = pgrest_setting.listener_backlog;
    listener->rcvbuf = pgrest_setting.listener_tcp_rcvbuf;
    listener->sndbuf = pgrest_setting.listener_tcp_sndbuf;

#ifdef HAVE_KEEPALIVE_TUNABLE
    listener->keepidle = pgrest_setting.listener_tcp_keepidle;
    listener->keepintvl = pgrest_setting.listener_tcp_keepintvl;
    listener->keepcnt = pgrest_setting.listener_tcp_keepcnt;
#endif

#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
    listener->ipv6only = pgrest_setting.listener_ipv6only;
#endif
#if defined(HAVE_DEFERRED_ACCEPT) && defined(SO_ACCEPTFILTER)
    listener->accept_filter = pstrdup(pgrest_setting.acceptor_accept_filter);
    if (strlen(listener->accept_filter)) {
        listener->add_deferred = 1;
    }
#endif
#if defined(HAVE_DEFERRED_ACCEPT) && defined(TCP_DEFER_ACCEPT)
    listener->deferred_accept = pgrest_setting.acceptor_deferred_accept;
    if (listener->deferred_accept) {
        listener->add_deferred = 1;
    }
#endif
#ifdef HAVE_SETFIB
    listener->setfib = pgrest_setting.acceptor_setfib;
#endif
#ifdef HAVE_TCP_FASTOPEN
    listener->fastopen = pgrest_setting.acceptor_fastopen;
#endif
#ifdef HAVE_REUSEPORT
    listener->reuseport = pgrest_setting.acceptor_reuseport;
#endif
#ifdef HAVE_UNIX_SOCKETS
    if (listener->sockaddr->sa_family == AF_UNIX) {
        listener->reuseport = 0;
    }
#endif

    listener->post_accept_timeout = pgrest_setting.acceptor_timeout;
    listener->open = 1;

    if (pg_strcasecmp(pgrest_setting.listener_tcp_keepalive, "on") == 0) {
        listener->keepalive = 1;
    } else if (pg_strcasecmp(pgrest_setting.listener_tcp_keepalive, "off") == 0) {
        listener->keepalive = 2;
    } else {
        listener->keepalive = 0;
    }

    return listener;
}

List * 
pgrest_listener_add(int family, const char *host_name,
                         unsigned short port_number, 
                         const char *unix_socket_dir,
                         pgrest_conn_handler_pt conn_handler)
{
    int                 result;
    char                port_number_str[32];
    char               *service;
    struct addrinfo    *addrs = NULL;
    struct addrinfo    *addr = NULL;
    struct addrinfo     hint;
#ifdef HAVE_UNIX_SOCKETS
    char                unix_socket_path[MAXPGPATH];
#endif
    pgrest_listener_t  *listener;
    List               *listeners = NIL; 

    /* Initialize hint structure */
    MemSet(&hint, 0, sizeof(hint));
    hint.ai_family = family;
    hint.ai_flags = AI_PASSIVE;
    hint.ai_socktype = SOCK_STREAM;

    switch (family) {
#ifdef HAVE_UNIX_SOCKETS
    case AF_UNIX:
        /*
         * Create unixSocketPath from portNumber and unixSocketDir
         */
        PGREST_UNIXSOCK_PATH(unix_socket_path, port_number, unix_socket_dir);
        if (strlen(unix_socket_path) >= UNIXSOCK_PATH_BUFLEN) {
            ereport(WARNING,
                    (errmsg(PGREST_PACKAGE " " "unix-domain socket path \"%s\" "
                            "is too long (maximum %d bytes)",
                             unix_socket_path,
                             (int) (UNIXSOCK_PATH_BUFLEN - 1))));
			return listeners;
		}

		service = unix_socket_path;
        break;
#endif

    default: /* AF_INET */
		snprintf(port_number_str, sizeof(port_number_str), "%d", port_number);
		service = port_number_str;
    }

    result = pg_getaddrinfo_all(host_name, service, &hint, &addrs);
    if (result || !addrs) {
        if (host_name) {
            ereport(WARNING,
                    (errmsg(PGREST_PACKAGE " " "could not translate host "
                            "name \"%s\", service \"%s\" to address: %s",
                            host_name, service, gai_strerror(result))));
        } else {
            ereport(WARNING,
                    (errmsg(PGREST_PACKAGE " " "could not translate "
                            "service \"%s\" to address: %s",
                            service, gai_strerror(result))));
        }

        if (addrs) {
            pg_freeaddrinfo_all(hint.ai_family, addrs);
        }

        return listeners;
    }

    for (addr = addrs; addr; addr = addr->ai_next) {
        if (!IS_AF_UNIX(family) && IS_AF_UNIX(addr->ai_family)) {
            continue;
        }

        listener = pgrest_listener_create(addr->ai_addr, addr->ai_addrlen,
                                          conn_handler);

        listeners = lappend(listeners, listener);
    }

    pg_freeaddrinfo_all(hint.ai_family, addrs);
    return listeners;
}

static bool
pgrest_listener_open(List *listeners)
{
    int                reuseaddr = 1;
    evutil_socket_t    fd;
    ListCell          *cell;

    /* for each listening socket */
    foreach(cell, listeners) {
        pgrest_listener_t *listener = lfirst(cell);

        if (listener->fd != (evutil_socket_t) -1) {
            continue;
        }

        fd = socket(listener->sockaddr->sa_family, listener->type, 0);

        if (fd == (evutil_socket_t) -1) {
			ereport(LOG,
					(errcode_for_socket_access(),
					 errmsg(PGREST_PACKAGE " " "could not create %s socket: %m",
							listener->addr_text)));
            return false;
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                       (const void *) &reuseaddr, sizeof(int)) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "setsockopt(SO_REUSEADDR) "
                            "for %s failed: %m", listener->addr_text)));
            evutil_closesocket(fd);
            return false;
        }

#ifdef HAVE_REUSEPORT
        if (listener->reuseport) {
            int reuseport = 1;

            if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                           (const void *) &reuseport, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_REUSEPORT) "
                                "for %s failed: %m", listener->addr_text)));
                evutil_closesocket(fd);
                return false;
            }
        }
#endif

#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
        if (listener->sockaddr->sa_family == AF_INET6) {
            int ipv6only = listener->ipv6only;

            if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                           (const void *) &ipv6only, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(IPV6_V6ONLY) "
                                "for %s failed: %m,ignored", listener->addr_text)));
            }
        }
#endif

        if (evutil_make_socket_nonblocking(fd) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "make socket nonblocking "
                            "for %s failed: %m", listener->addr_text)));
            evutil_closesocket(fd);
            return false;
        }

        if (bind(fd, listener->sockaddr, listener->socklen) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "could not bind to %s failed: %m",
                            listener->addr_text),
                     (IS_AF_UNIX(listener->sockaddr->sa_family)) ?
                  errhint("Is another pg_rest already running on port %s? "
                          "If not, remove socket file \"%s\" and retry.",
                          listener->addr_text, listener->addr_text) :
                  errhint("Is another pg_rest already running on port %s? "
                          "If not, wait a few seconds and retry.",
                          listener->addr_text)));
            evutil_closesocket(fd);
            return false;
        }

#ifdef HAVE_UNIX_DOMAIN
        if (listener->sockaddr->sa_family == AF_UNIX) {
            mode_t   mode;
            mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

            if (chmod(listener->addr_text, mode) == -1) {
                ereport(LOG,
                        (errcode_for_file_access(),
                         errmsg(PGREST_PACKAGE " " "could not set permissions of"
                                " file \"%s\": %m", listener->addr_text)));
            }
        }
#endif

        if (listen(fd, listener->backlog) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "could not listen on %s, "
                            "backlog %d : %m", listener->addr_text, listener->backlog)));
            evutil_closesocket(fd);
            return false;
        }

        listener->listen = 1;
        listener->fd = fd;
    }

    return true;
}

static void
pgrest_listener_configure(List *listeners)
{
    int                        value;
    ListCell                  *cell;
#if defined(HAVE_DEFERRED_ACCEPT) && defined(SO_ACCEPTFILTER)
    struct accept_filter_arg   af;
#endif

    /* for each listening socket */
    foreach(cell, listeners) {
        pgrest_listener_t *listener = lfirst(cell);

        if (listener->rcvbuf != -1) {
            if (setsockopt(listener->fd, SOL_SOCKET, SO_RCVBUF,
                           (const void *) &listener->rcvbuf, sizeof(int)) == -1 ) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_RCVBUF, %d) "
                                "for %s failed : %m ignored",
                         listener->rcvbuf, listener->addr_text)));
            }
        }

        if (listener->sndbuf != -1) {
            if (setsockopt(listener->fd, SOL_SOCKET, SO_SNDBUF,
                           (const void *) &listener->sndbuf, sizeof(int)) == -1 ) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_SNDBUF, %d) "
                                "for %s failed : %m ignored",
                         listener->sndbuf, listener->addr_text)));
            }
        }

        if (listener->keepalive) {
            value = (listener->keepalive == 1) ? 1 : 0;

            if (setsockopt(listener->fd, SOL_SOCKET, SO_KEEPALIVE,
                           (const void *) &value, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_KEEPALIVE, %d) "
                                "for %s failed : %m ignored",
                         value, listener->addr_text)));
            }
        }

#ifdef HAVE_KEEPALIVE_TUNABLE
        if (listener->keepidle) {
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_KEEPIDLE,
                           (const void *) &listener->keepidle, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_KEEPIDLE, %d) "
                                "for %s failed : %m ignored",
                         listener->keepidle, listener->addr_text)));
            }
        }

        if (listener->keepintvl) {
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_KEEPINTVL,
                           (const void *) &listener->keepintvl, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_KEEPINTVL, %d) "
                                "for %s failed : %m ignored",
                         listener->keepintvl, listener->addr_text)));
            }
        }

        if (listener->keepcnt) {
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_KEEPCNT,
                           (const void *) &listener->keepcnt, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_KEEPCNT, %d) "
                                "for %s failed : %m ignored",
                         listener->keepcnt, listener->addr_text)));
            }
        }
#endif

#ifdef HAVE_SETFIB
        if (listener->setfib != -1) {
            if (setsockopt(listener->fd, SOL_SOCKET, SO_SETFIB,
                           (const void *) &listener->setfib, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_SETFIB, %d) "
                                "for %s failed : %m ignored",
                         listener->setfib, listener->addr_text)));
            }
        }
#endif

#ifdef HAVE_TCP_FASTOPEN
        if (listener->fastopen != -1) {
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_FASTOPEN,
                           (const void *) &listener->fastopen, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_FASTOPEN, %d) "
                                "for %s failed : %m ignored",
                         listener->fastopen, listener->addr_text)));
            }
        }
#endif

#ifdef HAVE_DEFERRED_ACCEPT

#ifdef SO_ACCEPTFILTER
        if (listener->add_deferred) {
            MemSet(&&af, 0, sizeof(accept_filter_arg));
            StrNCpy(af.af_name, listener->accept_filter,16);

            if (setsockopt(listener->fd, SOL_SOCKET, SO_ACCEPTFILTER,
                           &af, sizeof(struct accept_filter_arg)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_ACCEPTFILTER, \"%s\") "
                                "for %s failed : %m ignored",
                         listener->accept_filter, listener->addr_text)));
                continue;
            }

            listener->deferred_accept = 1;
        }
#endif

#ifdef TCP_DEFER_ACCEPT
        if (listener->add_deferred) {
            value = 1;
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_DEFER_ACCEPT,
                           &value, sizeof(int)) == -1) {
                 ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_DEFER_ACCEPT, %d) "
                                "for %s failed : %m ignored",
                         value, listener->addr_text)));
                continue;
            }
        }

        if (listener->add_deferred) {
            listener->deferred_accept = 1;
        }
#endif

#endif /* HAVE_DEFERRED_ACCEPT */
    }
}

static void
pgrest_listener_close(void)
{
    ListCell            *cell;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;

    pgrest_acceptor_use_mutex = false;
    pgrest_acceptor_mutex_held = false;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "listener close")));

    foreach(cell, pgrest_listener_listeners) {
        listener = lfirst(cell);
        conn = listener->connection;

        if (conn) {
            /* worker only  */
            (void) pgrest_event_del(listener->ev, EV_READ);
            pgrest_conn_free(conn);
            conn->fd = (evutil_socket_t) -1;
        }

        ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "%s close listener %s %d", 
                                !IsUnderPostmaster ? "postmaster" : "worker", 
                                listener->addr_text, 
                                listener->fd)));

        closesocket(listener->fd);

#ifdef HAVE_UNIX_SOCKETS
        if (listener->sockaddr->sa_family == AF_UNIX && !IsUnderPostmaster) {
            (void) unlink(listener->addr_text);
            debug_log(DEBUG1,(errmsg(PGREST_PACKAGE " " "unlink unix domain socket")));
        }
#endif
        listener->fd = (evutil_socket_t) -1;
    }
}

bool
pgrest_listener_resume(void)
{
    ListCell            *cell;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;
    struct event        *ev;

    foreach(cell, pgrest_listener_listeners) {
        listener = lfirst(cell);
        conn = listener->connection;
        ev = listener->ev;

        if (conn == NULL || ev == NULL) {
            continue;
        }

        if(!pgrest_event_add(ev, EV_READ, 0)) {
            return false;
        }
    }

    return true;
}

bool
pgrest_listener_pause(bool all)
{
    ListCell            *cell;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;
    struct event        *ev;

    foreach(cell, pgrest_listener_listeners) {
        listener = lfirst(cell);
        conn = listener->connection;
        ev = listener->ev;

        if (conn == NULL || ev == NULL) {
            continue;
        }
#ifdef HAVE_REUSEPORT
        if (listener->reuseport && !all) {
            continue;
        }
#endif

        if (!pgrest_event_del(ev, EV_READ)) {
            return false;
        }
    }

    return true;
}

void
pgrest_listener_hook_add(pgrest_listener_hook_pt init, void *data) 
{
    pgrest_listener_hook_t  *hook;

    hook = (pgrest_listener_hook_t *) palloc0fast(sizeof(pgrest_listener_hook_t));

    hook->init = init;
    hook->data = data;

    pgrest_listener_hooks = lappend(pgrest_listener_hooks, hook);
}

static bool
pgrest_listener_worker_init(void *data)
{
    ListCell            *cell;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;

    /* for each listening socket */
    foreach(cell, pgrest_listener_listeners) {
        listener = lfirst(cell);

        conn = pgrest_conn_get(listener->fd, false);
        if (conn == NULL) {
            return false;
        }

        conn->type = listener->type;
        conn->listener = listener;

        listener->connection = conn;
        listener->ev = pgrest_event_new(pgrest_worker_event_base,
                                 listener->fd,
                                 EV_READ | EV_PERSIST,
                                 pgrest_acceptor_accept_handler,
                                 conn);

        if (listener->ev == NULL) {
            ereport(WARNING, (errmsg(PGREST_PACKAGE " " "worker %d create event "
                                     "object failed", pgrest_worker_index)));
            return false;
        }

#ifdef HAVE_REUSEPORT
        if (listener->reuseport) {
            pgrest_event_priority_set(listener->ev, pgrest_acceptor_use_mutex ? 1 : 0);
            if (!pgrest_event_add(listener->ev, EV_READ, 0)) {
                return false;
            }
            continue;
        }
#endif
        pgrest_event_priority_set(listener->ev, 0);

        if (pgrest_acceptor_use_mutex) {
            continue;
        }

        if (!pgrest_event_add(listener->ev, EV_READ, 0)) {
            return false;
        }
    }

    return true;
}

static bool
pgrest_listener_worker_exit(void *data)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d listener_exit",
                               pgrest_worker_index)));

    pgrest_listener_close();
    return true;
}

static StringInfo
pgrest_listener_info_format(List *listeners)
{
    StringInfo             result;
    ListCell              *cell;
    pgrest_listener_t     *listener;

    result = makeStringInfo();

    /* for each listening socket */
    foreach(cell, listeners) {
        listener = lfirst(cell);

        appendStringInfo(result,"\n");

        appendStringInfo(result,
            "!\tlistener addr:%s\n", listener->addr_text);
        appendStringInfo(result,
            "!\tlistener open:%d\n", listener->open);
#if (HAVE_REUSEPORT)
        appendStringInfo(result,
            "!\tlistener reuseport:%d\n", listener->reuseport);
#endif

#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
        appendStringInfo(result,
            "!\tlistener addr:%d\n", listener->ipv6only);
#endif

        appendStringInfo(result,
            "!\tlistener listen:%d\n", listener->listen);
        appendStringInfo(result,
            "!\tlistener post_accept_timeout:%d\n", listener->post_accept_timeout);
        appendStringInfo(result,
            "!\tlistener backlog:%d\n", listener->backlog);
        appendStringInfo(result,
            "!\tlistener sndbuf:%d\n", listener->sndbuf);
        appendStringInfo(result,
            "!\tlistener keepalive:%d\n", listener->keepalive);
        appendStringInfo(result,
            "!\tlistener rcvbuf:%d\n", listener->rcvbuf);

#if defined(HAVE_DEFERRED_ACCEPT) && defined(SO_ACCEPTFILTER)
        appendStringInfo(result,
            "!\tlistener accept_filter:%s\n", listener->accept_filter);
#endif
#if defined(HAVE_DEFERRED_ACCEPT) && defined(TCP_DEFER_ACCEPT)
        appendStringInfo(result,
            "!\tlistener deferred_accept:%d\n", listener->deferred_accept);
#endif
#ifdef HAVE_SETFIB
        appendStringInfo(result,
            "!\tlistener setfib:%d\n", listener->setfib);
#endif
#ifdef HAVE_TCP_FASTOPEN
        appendStringInfo(result,
            "!\tlistener fastopen:%d\n", listener->fastopen);
#endif

    }

    return result;
}

void
pgrest_listener_info_print(void)
{
    StringInfo buffer;

    buffer = pgrest_listener_info_format(pgrest_listener_listeners);

    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print %s listener ++++++++++", 
                            !IsUnderPostmaster ? "postmaster" : "worker")));
    ereport(DEBUG1,(errmsg("\n%s", buffer->data)));
    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print %s listener ----------", 
                            !IsUnderPostmaster ? "postmaster" : "worker")));

    pfree(buffer->data);
    pfree(buffer);
}

static void
pgrest_listener_fini1(int status, Datum arg)
{
    pgrest_listener_fini();
}

void
pgrest_listener_init(void)
{
    ListCell         *cell;

    foreach(cell, pgrest_listener_hooks) {
        pgrest_listener_hook_t *hook = lfirst(cell);
        if (hook->init) {
            pgrest_listener_listeners = list_concat(pgrest_listener_listeners, 
                                                    hook->init(hook->data));
        }
    }

    if (pgrest_listener_listeners == NIL) {
        ereport(FATAL,
                (errmsg(PGREST_PACKAGE " " "could not create any listener")));
    }

    if (pgrest_listener_open(pgrest_listener_listeners) == false) {
        ereport(FATAL,
                (errmsg(PGREST_PACKAGE " " "failed to open socket listener")));
    }

    pgrest_listener_configure(pgrest_listener_listeners);

    pgrest_worker_hook_add(pgrest_listener_worker_init, pgrest_listener_worker_exit, "listener", NULL, false);

    on_proc_exit(pgrest_listener_fini1, 0);
#if PGREST_DEBUG
    pgrest_listener_info_print();
#endif
}

void
pgrest_listener_fini(void)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "listener fini")));
    pgrest_listener_close();
}
