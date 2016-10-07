/* -------------------------------------------------------------------------
 *
 * pq_rest_conf.c
 *   pg_rest configure file parser
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_conf.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct pgrest_conf_command_s    pgrest_conf_command_t;
typedef void  *(*pgrest_conf_create_pt) (void *);
typedef void   (*pgrest_conf_set_pt)    (pgrest_conf_command_t *, void *, void *);

struct pgrest_conf_command_s {
    const char            *name;
    size_t                 offset;
    int                    min_val;
    int                    max_val;
    pgrest_conf_create_pt  create_conf;
    pgrest_conf_set_pt     set;
};

static void *pgrest_conf_setting_create(void *parent);
static void *pgrest_conf_http_server_create(void *parent);
static void *pgrest_conf_listener_create(void *parent);
static void  pgrest_conf_parse_object(const char *name, 
                                 json_t *jelem, 
                                 void *parent);
static void  pgrest_conf_integer_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_bool_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_string_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_address_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_bind_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_integer_set2(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_bool_set2(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_afilter_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_deferred_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_ipv6only_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_keepalive_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void pgrest_conf_keepalive_opt_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_server_name_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_reuse_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
static void  pgrest_conf_ssl_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);

static pgrest_conf_command_t  pgrest_conf_commands[] = {

    { "root",
      0,
      0,
      0,
      pgrest_conf_setting_create,
      NULL },

    { "worker",
      0,
      0,
      0,
      NULL,
      NULL },

    { "worker_processes",
      offsetof(pgrest_setting_t, worker_processes),
      1,
      PGREST_MAX_WORKERS,
      NULL,
      pgrest_conf_integer_set },

    { "worker_priority",
      offsetof(pgrest_setting_t, worker_priority),
      -20,
      20,
      NULL,
      pgrest_conf_integer_set },

    { "worker_nofile",
      offsetof(pgrest_setting_t, worker_nofile),
      -1,
      65535,
      NULL,
      pgrest_conf_integer_set },

    { "worker_connections",
      offsetof(pgrest_setting_t, worker_noconn),
      1,
      65535,
      NULL,
      pgrest_conf_integer_set },

    { "accept",
      0,
      0,
      0,
      NULL,
      NULL },

    { "accept_mutex",
      offsetof(pgrest_setting_t, acceptor_mutex),
      0,
      0,
      NULL,
      pgrest_conf_bool_set },

    { "accept_mutex_delay",
      offsetof(pgrest_setting_t, acceptor_mutex_delay),
      500,
      600000,
      NULL,
      pgrest_conf_integer_set },

    { "multi_accept",
      offsetof(pgrest_setting_t, acceptor_multi_accept),
      0,
      0,
      NULL,
      pgrest_conf_bool_set },

    { "http",
      0,
      0,
      0,
      NULL,
      NULL },

    { "tcp_nopush",
      offsetof(pgrest_setting_t, http_tcp_nopush),
      0,
      0,
      NULL,
      pgrest_conf_bool_set },

    { "keepalive_timeout",
      offsetof(pgrest_setting_t, http_keepalive_timeout),
      1000,
      3600000,
      NULL,
      pgrest_conf_integer_set },

    { "server",
      0,
      0,
      0,
      pgrest_conf_http_server_create,
      NULL },

    { "listen",
      0,
      0,
      0,
      pgrest_conf_listener_create,
      NULL },

    { "address",
      offsetof(pgrest_conf_listener_t, sockaddr),
      0,
      0,
      NULL,
      pgrest_conf_address_set },

    { "default_server",
      offsetof(pgrest_conf_listener_t, default_server),
      0,
      0,
      NULL,
      pgrest_conf_bool_set },

    { "bind",
      offsetof(pgrest_conf_listener_t, bind),
      0,
      0,
      NULL,
      pgrest_conf_bool_set2 },

    { "backlog",
      offsetof(pgrest_conf_listener_t, backlog),
      1,
      65535,
      NULL,
      pgrest_conf_integer_set2 },

    { "sndbuf",
      offsetof(pgrest_conf_listener_t, sndbuf),
      -1,
      65535,
      NULL,
      pgrest_conf_integer_set2 },

    { "rcvbuf",
      offsetof(pgrest_conf_listener_t, rcvbuf),
      -1,
      65535,
      NULL,
      pgrest_conf_integer_set2 },

    { "so_keepalive",
      offsetof(pgrest_conf_listener_t, so_keepalive),
      0,
      0,
      NULL,
      pgrest_conf_keepalive_set },

    { "tcp_keepidle",
      offsetof(pgrest_conf_listener_t, tcp_keepidle),
      0,
      INT_MAX,
      NULL,
      pgrest_conf_keepalive_opt_set },

    { "tcp_keepintvl",
      offsetof(pgrest_conf_listener_t, tcp_keepintvl),
      0,
      INT_MAX,
      NULL,
      pgrest_conf_keepalive_opt_set },

    { "tcp_keepcnt",
      offsetof(pgrest_conf_listener_t, tcp_keepcnt),
      0,
      INT_MAX,
      NULL,
      pgrest_conf_keepalive_opt_set },

    { "ipv6_only",
      offsetof(pgrest_conf_listener_t, ipv6only),
      0,
      0,
      NULL,
      pgrest_conf_ipv6only_set },

    { "accept_filter",
      offsetof(pgrest_conf_listener_t, accept_filter),
      0,
      0,
      NULL,
      pgrest_conf_afilter_set },

    { "deferred_accept",
      offsetof(pgrest_conf_listener_t, deferred_accept),
      0,
      0,
      NULL,
      pgrest_conf_deferred_set },

    { "setfib",
      offsetof(pgrest_conf_listener_t, setfib),
      -1,
      INT_MAX,
      NULL,
      pgrest_conf_integer_set2 },

    { "fastopen",
      offsetof(pgrest_conf_listener_t, fastopen),
      -1,
      INT_MAX,
      NULL,
      pgrest_conf_integer_set2 },

    { "reuseport",
      offsetof(pgrest_conf_listener_t, reuseport),
      0,
      0,
      NULL,
      pgrest_conf_reuse_set },

    { "ssl",
      offsetof(pgrest_conf_listener_t, ssl),
      0,
      0,
      NULL,
      pgrest_conf_ssl_set },

    { "http2",
      offsetof(pgrest_conf_listener_t, http2),
      0,
      0,
      NULL,
      pgrest_conf_bool_set },

    { "server_name",
      offsetof(pgrest_conf_http_server_t, server_name),
      0,
      0,
      NULL,
      pgrest_conf_server_name_set },

    { "crud_prefix_url",
      offsetof(pgrest_conf_http_server_t, crud_prefix_url),
      0,
      0,
      NULL,
      pgrest_conf_string_set },

    { "sql_prefix_url",
      offsetof(pgrest_conf_http_server_t, sql_prefix_url),
      0,
      0,
      NULL,
      pgrest_conf_string_set },

    { "doc_prefix_url",
      offsetof(pgrest_conf_http_server_t, doc_prefix_url),
      0,
      0,
      NULL,
      pgrest_conf_string_set },

#ifdef HAVE_OPENSSL
    { "ssl_certificate",
      offsetof(pgrest_conf_http_server_t, ssl_certificate),
      0,
      0,
      NULL,
      pgrest_conf_string_set },

    { "ssl_certificate_key",
      offsetof(pgrest_conf_http_server_t, ssl_certificate_key),
      0,
      0,
      NULL,
      pgrest_conf_string_set },

    { "ssl_session_cache",
      offsetof(pgrest_conf_http_server_t, ssl_session_cache),
      0,
      0,
      NULL,
      pgrest_conf_bool_set },

    { "ssl_session_timeout",
      offsetof(pgrest_conf_http_server_t, ssl_session_timeout),
      60000,
      3600000,
      NULL,
      pgrest_conf_integer_set },
#endif

    { "client_header_timeout",
      offsetof(pgrest_conf_http_server_t, client_header_timeout),
      1000,
      3600000,
      NULL,
      pgrest_conf_integer_set },

    { NULL, 0, 0, 0, NULL, NULL }
};

static pgrest_setting_t  *pgrest_setting_private;

static void
pgrest_conf_integer_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    int   *newval = val;
    char  *object = parent;
    int   *save;

    if (*newval < cmd->min_val || *newval > cmd->max_val) {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value for directive "
                        "\"%s\": \"%d\" must be in the range from %d to %d",
                         cmd->name, *newval, cmd->min_val, cmd->max_val)));
    }

    save = (int *) (object + cmd->offset);
    *save = *newval;
}

static void
pgrest_conf_bool_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    bool  *newval = val;
    char  *object = parent;
    bool  *save;

    if (*newval != 0 && *newval != 1) {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value for directive "
                        "\"%s\": \"%d\" only accept \"true\" or \"false\"",
                         cmd->name, *newval)));
    }

    save = (bool *) (object + cmd->offset);
    *save = *newval;
}

static void
pgrest_conf_string_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    char  *newval = val;
    char  *object = parent;
    char **save;

    save = (char **) (object + cmd->offset);
    *save = pstrdup(newval);
}

static void  
pgrest_conf_address_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_conf_listener_t *conf_listener = parent;
    char                   *value = val;
    pgrest_url_t            u;

    MemSet(&u, 0, sizeof(pgrest_url_t));

    u.url = value;
    u.url_len = strlen(value);
    u.listen = 1;
    u.default_port = 8080;

    if (!pgrest_util_parse_url(&u)) {
        if (u.err) {
            ereport(ERROR, 
                    (errmsg(PGREST_PACKAGE " " "invalid value \"%s\" for "
                            "directive \"%s\": \"%s\"", value, cmd->name, u.err)));
        }
    }

    memcpy(&conf_listener->sockaddr.sockaddr, &u.sockaddr, u.socklen);

    conf_listener->socklen = u.socklen;
    conf_listener->wildcard = u.wildcard;

    (void) pgrest_util_inet_ntop(&conf_listener->sockaddr.sockaddr, 
                                 conf_listener->socklen, 
                                 conf_listener->addr,
                                 PGREST_SOCKADDR_STRLEN, 
                                 true);
}

static void  
pgrest_conf_bind_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_conf_listener_t *conf_listener = parent;
    bool                   *value = val;

    if (*value == true) {
        conf_listener->set = *value;
        conf_listener->bind = *value;
    }
}

static void  
pgrest_conf_integer_set2(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    bool value = true;

    pgrest_conf_bind_set(cmd, &value, parent);
    pgrest_conf_integer_set(cmd, val, parent);
}

#if 0
static void 
pgrest_conf_string_set2(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    char                   *value = val;
    bool                    set = false;

    if (value && value[0] != '\0') {
        set = true;            
    }
    pgrest_conf_bind_set(cmd, &set, parent);
    pgrest_conf_string_set(cmd, val, parent);
}
#endif

static void  
pgrest_conf_bool_set2(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_conf_bind_set(cmd, val, parent);
    pgrest_conf_bool_set(cmd, val, parent);
}

static void  
pgrest_conf_afilter_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_conf_listener_t *conf_listener = parent;
    char                   *value = val;
    bool                    set = true;

    if (value && value[0] != '\0') {
#if !defined(HAVE_DEFERRED_ACCEPT) || !defined(SO_ACCEPTFILTER)
        ereport(WARNING, 
                (errmsg(PGREST_PACKAGE " " "directive \"%s\" is not supported "
                        "on this platform, ignored", cmd->name)));
        return;
#endif
    }

    pgrest_conf_bind_set(cmd, &set, parent);
    StrNCpy(conf_listener->accept_filter, value, PGREST_AF_SIZE);
}

static void  
pgrest_conf_deferred_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    bool *value = val;

    if (*value == true) {
#if !defined(HAVE_DEFERRED_ACCEPT) || !defined(TCP_DEFER_ACCEPT)
        ereport(WARNING, 
                (errmsg(PGREST_PACKAGE " " "directive \"%s\" is not supported "
                        "on this platform, ignored", cmd->name)));
        return;
#endif
    }
    pgrest_conf_bool_set2(cmd, val, parent);
}

static void  
pgrest_conf_ipv6only_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    bool *value = val;

    if (*value == true) {
#if !defined(HAVE_IPV6) || !defined(IPV6_V6ONLY)
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "directive \"%s\" is not supported "
                        "on this platform", cmd->name)));
#endif
    }
    pgrest_conf_bool_set2(cmd, val, parent);
}

static void
pgrest_conf_reuse_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    bool *newval = val;

    if (*newval == true) {
#ifndef HAVE_REUSEPORT
        ereport(WARNING, 
                (errmsg(PGREST_PACKAGE " " "directive \"%s\" is not supported "
                        "on this platform, ignored", cmd->name)));
        return;
#endif
    }
    pgrest_conf_bool_set2(cmd, val, parent);
}

static void
pgrest_conf_ssl_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    bool *newval = val;

    if (*newval == true) {
#ifndef HAVE_OPENSSL
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "directive \"%s\" is not supported : "
                        "please rebuild " PGREST_PACKAGE " "
                        "--with-openssl", cmd->name)));
#endif
    }
    pgrest_conf_bool_set(cmd, val, parent);
}

static void  
pgrest_conf_keepalive_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_conf_listener_t *conf_listener = parent;
    char                   *value = val;
    bool                    set = true;

    if (strcmp(value, "on") == 0) {
        conf_listener->so_keepalive = 1;
    } else if (strcmp(value, "off") == 0) {
        conf_listener->so_keepalive = 2;
    } else {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value for directive "
                        "\"%s\": \"%s\" only accept \"on\" or \"off\"",
                         cmd->name, value)));
    }

    pgrest_conf_bind_set(cmd, &set, parent);
}

static void  
pgrest_conf_keepalive_opt_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_conf_listener_t *conf_listener = parent;
#ifdef HAVE_KEEPALIVE_TUNABLE
    conf_listener->so_keepalive = 1;
    pgrest_conf_integer_set2(cmd, val, parent);
#else
    ereport(WARNING, 
            (errmsg(PGREST_PACKAGE " " "directive \"%s\" is not supported "
                    "on this platform, ignored", cmd->name)));
#endif
}

static void  
pgrest_conf_server_name_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_conf_http_server_t *conf_http_server = parent;
    char                      *value = val;
    char                      *raw_string;
    List                      *elemlist;
    ListCell                  *cell;
    pgrest_http_server_name_t *sn;

    if (!value || value[0] == '\0') {
        return ;
    }

    raw_string = pstrdup(value);
    if (!SplitIdentifierString(raw_string, ',', &elemlist)) {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value for directive "
                        "\"%s\": \"%s\" only accept comma-separated values",
                         cmd->name, value)));
    }

    foreach(cell, elemlist) {
        char *name = (char *) lfirst(cell);

        sn = pgrest_array_push(&conf_http_server->server_names);
        sn->server = conf_http_server;
        StrNCpy(sn->name, name, sizeof(sn->name));
    }

    list_free(elemlist);
    pfree(raw_string);
}

static void *
pgrest_conf_setting_create(void *parent)
{
    pgrest_setting_private = palloc0fast(sizeof(pgrest_setting_t));

    pgrest_setting_private->worker_processes = 4;
    pgrest_setting_private->worker_priority = 0;
    pgrest_setting_private->worker_nofile = 1024;
    pgrest_setting_private->worker_noconn = 1024;
    pgrest_setting_private->acceptor_mutex = true;
    pgrest_setting_private->acceptor_mutex_delay = 500;
    pgrest_setting_private->acceptor_multi_accept = false;
    pgrest_setting_private->http_tcp_nopush = false;
    pgrest_setting_private->http_keepalive_timeout = 600000;

    pgrest_array_init(&pgrest_setting_private->conf_http_servers,
                      4, 
                      sizeof(pgrest_conf_http_server_t));
    pgrest_array_init(&pgrest_setting_private->conf_listeners, 
                      4, 
                      sizeof(pgrest_conf_listener_t));

    return pgrest_setting_private;
}

static void *
pgrest_conf_http_server_create(void *parent)
{
    pgrest_setting_t          *setting = parent;
    pgrest_conf_http_server_t *conf_http_server;

    conf_http_server = pgrest_array_push(&setting->conf_http_servers);

    conf_http_server->server_name = "";
    conf_http_server->crud_prefix_url = "/crud/";
    conf_http_server->sql_prefix_url = "/sql/";
    conf_http_server->doc_prefix_url = "/doc/";
#ifdef HAVE_OPENSSL
    conf_http_server->ssl_certificate = "cert.pem";
    conf_http_server->ssl_certificate_key = "ssl_certificate_key";
    conf_http_server->ssl_session_cache = true;
    conf_http_server->ssl_session_timeout = 1800000;
#endif
    conf_http_server->client_header_timeout = 5000;

    pgrest_array_init(&conf_http_server->server_names, 
                      2, 
                      sizeof(pgrest_http_server_name_t));

    conf_http_server->listen = 0;
    conf_http_server->conf_main = setting;

    return conf_http_server;
}

static void *
pgrest_conf_listener_create(void *parent)
{
    pgrest_conf_http_server_t *conf_http_server = parent;
    pgrest_setting_t          *setting;
    pgrest_conf_listener_t    *conf_listener;
    struct sockaddr_in        *sin;

    conf_http_server->listen = 1;

    setting = conf_http_server->conf_main;
    conf_listener = pgrest_array_push(&setting->conf_listeners);

    MemSet(conf_listener, 0, sizeof(pgrest_conf_listener_t));

    sin = &conf_listener->sockaddr.sockaddr_in;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(8080);
    sin->sin_addr.s_addr = INADDR_ANY;

    conf_listener->socklen = sizeof(struct sockaddr_in);
    conf_listener->wildcard = true;

    (void) pgrest_util_inet_ntop(&conf_listener->sockaddr.sockaddr, 
                                 conf_listener->socklen,
                                 conf_listener->addr, 
                                 PGREST_SOCKADDR_STRLEN, 
                                 true);

    conf_listener->backlog = DEFAUlT_PGREST_LISTEN_BACKLOG;
    conf_listener->sndbuf = -1;
    conf_listener->rcvbuf = -1;
#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
    conf_listener->ipv6only = true;
#endif
#ifdef HAVE_SETFIB
    conf_listener->setfib = -1;
#endif
#ifdef HAVE_TCP_FASTOPEN
    conf_listener->fastopen = -1;
#endif

    return conf_listener;
}

static void
pgrest_conf_copy_http_server(pgrest_conf_http_server_t *new_server, 
                             pgrest_conf_http_server_t *old_server)
{
    pgrest_http_server_name_t *sn;
    pgrest_uint_t              i;

    new_server->server_name = pstrdup(old_server->server_name);
    new_server->crud_prefix_url = pstrdup(old_server->crud_prefix_url);
    new_server->sql_prefix_url = pstrdup(old_server->sql_prefix_url);
    new_server->doc_prefix_url = pstrdup(old_server->doc_prefix_url);
#ifdef HAVE_OPENSSL
    new_server->ssl_certificate = pstrdup(old_server->ssl_certificate);
    new_server->ssl_certificate_key = pstrdup(old_server->ssl_certificate_key);
    new_server->ssl_session_cache = old_server->ssl_session_cache;
    new_server->ssl_session_timeout = old_server->ssl_session_timeout;
#endif
    new_server->client_header_timeout = old_server->client_header_timeout;
    new_server->listen = old_server->listen;

    pgrest_array_copy(&new_server->server_names, &old_server->server_names);

    sn = new_server->server_names.elts;
    for (i = 0; i < new_server->server_names.size; i++) {
        sn[i].server = new_server;
    }
}

static void
pgrest_conf_copy_http_servers(pgrest_array_t *dst, pgrest_array_t *src)
{
    pgrest_conf_http_server_t *old_server;
    pgrest_conf_http_server_t *new_server;
    pgrest_uint_t              i;

    old_server = src->elts;
    for (i = 0; i < src->size; i++) {
        new_server = pgrest_array_push(dst);
        pgrest_array_init(&new_server->server_names, 
                          2, 
                          sizeof(pgrest_http_server_name_t));
        pgrest_conf_copy_http_server(new_server, &old_server[i]);
    }
}

static void
pgrest_conf_copy_setting(pgrest_setting_t *new_setting, 
                         pgrest_setting_t *old_setting,
                         MemoryContext     mctx_old)
{
    MemoryContext mctx_save = MemoryContextSwitchTo(mctx_old);

    new_setting->worker_processes = old_setting->worker_processes;
    new_setting->worker_priority = old_setting->worker_priority;
    new_setting->worker_noconn = old_setting->worker_noconn;
    new_setting->worker_nofile = old_setting->worker_nofile;
    new_setting->acceptor_mutex = old_setting->acceptor_mutex;
    new_setting->acceptor_mutex_delay = old_setting->acceptor_mutex_delay;
    new_setting->acceptor_multi_accept = old_setting->acceptor_multi_accept;
    new_setting->http_tcp_nopush = old_setting->http_tcp_nopush;
    new_setting->http_keepalive_timeout = old_setting->http_keepalive_timeout;

    pgrest_array_copy(&new_setting->conf_listeners, &old_setting->conf_listeners);
    pgrest_conf_copy_http_servers(&new_setting->conf_http_servers, 
                                  &old_setting->conf_http_servers);

    MemoryContextSwitchTo(mctx_save);
}

static pgrest_conf_command_t *
pgrest_conf_cmd_find(const char *name)
{
    pgrest_conf_command_t  *cmd;

    for ( cmd = pgrest_conf_commands ; cmd->name; cmd++) {
        if (!strcmp(cmd->name, name)) {
            return cmd;
        }
    }

    ereport(ERROR, 
            (errmsg(PGREST_PACKAGE " " "unknown directive \"%s\"", name)));
}

static inline void 
pgrest_conf_parse_type(const char *name, void *val, void *parent)
{
    pgrest_conf_command_t       *cmd;

    cmd = pgrest_conf_cmd_find(name);
    Assert(cmd->set);
    cmd->set(cmd, val, parent);
}

static void 
pgrest_conf_parse_string(const char *name, json_t *jelem, void *parent)
{
    char *val = (char *) json_string_value(jelem);
    pgrest_conf_parse_type(name, val, parent);
}

static void
pgrest_conf_parse_integer(const char *name, json_t *jelem, void *parent)
{
    int val = (int)json_integer_value(jelem);
    pgrest_conf_parse_type(name, &val, parent);
}

static void
pgrest_conf_parse_bool(const char *name, bool value, void *parent)
{
    bool val = value;
    pgrest_conf_parse_type(name, &val, parent);
}

static void
pgrest_conf_parse_array(const char *name, json_t *jelem, void *parent)
{
    size_t                  i;
    pgrest_conf_command_t  *cmd;

    cmd = pgrest_conf_cmd_find(name);

    for (i = 0; i < json_array_size(jelem); ++i) {
        json_t *jnode = json_array_get(jelem, i);
        pgrest_conf_parse_object(cmd->name, jnode, parent);
    }
}

static void
pgrest_conf_parse_object(const char *name, json_t *jelem, void *parent)
{
    void                   *iter;
    pgrest_conf_command_t  *cmd;
    json_t                 *jnode;
    const char             *elem_name;
    void                   *object;

    cmd = pgrest_conf_cmd_find(name);
    object = parent;

    if (cmd->create_conf) {
        object = cmd->create_conf(parent);
    }

    for (iter = json_object_iter(jelem); 
         iter; 
         iter = json_object_iter_next(jelem, iter)) 
    {
        jnode = json_object_iter_value(iter);
        elem_name = json_object_iter_key(iter);

        switch (json_typeof(jnode)) {
        case JSON_STRING:
            pgrest_conf_parse_string(elem_name, jnode, object);
            break;
        case JSON_INTEGER:
            pgrest_conf_parse_integer(elem_name, jnode, object);
            break;
        case JSON_TRUE:
            pgrest_conf_parse_bool(elem_name, true, object);
            break;
        case JSON_FALSE:
            pgrest_conf_parse_bool(elem_name, false, object);
            break;
        case JSON_ARRAY:
            pgrest_conf_parse_array(elem_name, jnode, object);
            break;
        case JSON_OBJECT:
            pgrest_conf_parse_object(elem_name, jnode, object);
            break;
        default:
            break;
        }
    }
}

static void
pgrest_conf_default_values(pgrest_setting_t *setting)
{
    pgrest_conf_http_server_t *conf_http_server;

    if (!setting->conf_http_servers.size) {
        conf_http_server = pgrest_conf_http_server_create(setting);
        (void) pgrest_conf_listener_create(conf_http_server);
    }
}

static inline bool
pgrest_conf_values_check(pgrest_setting_t *setting)
{
    if(setting->conf_http_servers.size != setting->conf_listeners.size) {
        ereport(ERROR, (errmsg(PGREST_PACKAGE " " "one or more of the "
                        "\"http->server\" lacks a \"listen\" directive")));
        return false;
    }

    return true;
}

static bool
pgrest_conf_parse_init(const char *filename, 
                       json_t **jroot, 
                       MemoryContext *mctx_new,
                       MemoryContext *mctx_old)
{
    json_error_t            jerror;

    *jroot = json_load_file(filename, 0, &jerror);
    if (!*jroot) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "parse configure file: \"%s\" "
                                 "failed: %s line: %d", filename, jerror.text, 
                                  jerror.line)));
        return false;
    }

    *mctx_new = AllocSetContextCreate(TopMemoryContext,
                                      PGREST_PACKAGE " " "parser",
                                      ALLOCSET_SMALL_SIZES);
    *mctx_old = MemoryContextSwitchTo(*mctx_new);
    return true;
}

static void
pgrest_conf_parse_fini(json_t *jroot, 
                       MemoryContext mctx_new, 
                       MemoryContext mctx_old)
{
    mctx_old = MemoryContextSwitchTo(mctx_new);
    MemoryContextDelete(mctx_old);
    json_decref(jroot);
}

static bool
pgrest_conf_parse_iner(json_t *jroot, MemoryContext mctx)
{
    bool result = true;

    PG_TRY();
    {
        pgrest_conf_parse_object("root", jroot, NULL);
    }
    PG_CATCH();
    {
        EmitErrorReport();
        FlushErrorState();
        MemoryContextSwitchTo(mctx);
        result = false;
    }
    PG_END_TRY();

    return result;
}

bool 
pgrest_conf_parse(pgrest_setting_t *setting, const char *filename)
{
    MemoryContext           new_context;
    MemoryContext           old_context;
    json_t                 *jroot;

    
    if (!pgrest_conf_parse_init(filename, &jroot,
                                &new_context, &old_context)) {
        return false;
    }

    if (!pgrest_conf_parse_iner(jroot, new_context)) {
        pgrest_conf_parse_fini(jroot, old_context, new_context);
        return false;
    }

    /* setup default values if need */
    pgrest_conf_default_values(pgrest_setting_private);

    if (!pgrest_conf_values_check(pgrest_setting_private)) {
        pgrest_conf_parse_fini(jroot, old_context, new_context);
        return false;
    }

    /* TODO compare settings */

    /* copy setting */
    pgrest_conf_copy_setting(setting, pgrest_setting_private, old_context);

    /* release resource */
    pgrest_conf_parse_fini(jroot, old_context, new_context);
    return true;
}

void pgrest_conf_info_print(void)
{

}
