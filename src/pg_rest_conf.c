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

static void *pgrest_conf_setting_create(void *parent);
static void *pgrest_conf_http_server_create(void *parent);
static void *pgrest_conf_http_path_create(void *parent);
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
#ifdef HAVE_OPENSSL
static void  pgrest_conf_string_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);
#endif
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
static void  pgrest_conf_tbpath_set(pgrest_conf_command_t *cmd, 
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
static void  pgrest_conf_http_path_set(pgrest_conf_command_t *cmd, 
                                 void *val, 
                                 void *parent);

static pgrest_conf_command_t  pgrest_conf_static_cmds[] = {

    { "root",
      0,
      0,
      0,
      PGREST_CONF_OBJECT,
      pgrest_conf_setting_create },

    { "worker",
      0,
      0,
      0,
      PGREST_CONF_OBJECT,
      NULL },

    { "worker_processes",
      offsetof(pgrest_setting_t, worker_processes),
      1,
      PGREST_MAX_WORKERS,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set },

    { "worker_priority",
      offsetof(pgrest_setting_t, worker_priority),
      -20,
      20,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set },

    { "worker_nofile",
      offsetof(pgrest_setting_t, worker_nofile),
      -1,
      65535,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set },

    { "worker_connections",
      offsetof(pgrest_setting_t, worker_noconn),
      1,
      65535,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set },

    { "accept",
      0,
      0,
      0,
      PGREST_CONF_OBJECT,
      NULL },

    { "accept_mutex",
      offsetof(pgrest_setting_t, acceptor_mutex),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_bool_set },

    { "accept_mutex_delay",
      offsetof(pgrest_setting_t, acceptor_mutex_delay),
      500,
      600000,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set },

    { "multi_accept",
      offsetof(pgrest_setting_t, acceptor_multi_accept),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_bool_set },

    { "http",
      0,
      0,
      0,
      PGREST_CONF_OBJECT,
      NULL },

    { "tcp_nopush",
      offsetof(pgrest_setting_t, http_tcp_nopush),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_bool_set },

    { "keepalive_timeout",
      offsetof(pgrest_setting_t, http_keepalive_timeout),
      1000,
      3600000,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set },

    { "temp_buffer_path",
      offsetof(pgrest_setting_t, temp_buffer_path),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_tbpath_set },

    { "server",
      0,
      0,
      0,
      PGREST_CONF_OBJECT,
      pgrest_conf_http_server_create },

    { "listen",
      0,
      0,
      0,
      PGREST_CONF_OBJECT,
      pgrest_conf_listener_create },

    { "address",
      offsetof(pgrest_conf_listener_t, sockaddr),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_address_set },

    { "default_server",
      offsetof(pgrest_conf_listener_t, default_server),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_bool_set },

    { "bind",
      offsetof(pgrest_conf_listener_t, bind),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_bool_set2 },

    { "backlog",
      offsetof(pgrest_conf_listener_t, backlog),
      1,
      65535,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set2 },

    { "sndbuf",
      offsetof(pgrest_conf_listener_t, sndbuf),
      -1,
      65535,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set2 },

    { "rcvbuf",
      offsetof(pgrest_conf_listener_t, rcvbuf),
      -1,
      65535,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set2 },

    { "so_keepalive",
      offsetof(pgrest_conf_listener_t, so_keepalive),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_keepalive_set },

    { "tcp_keepidle",
      offsetof(pgrest_conf_listener_t, tcp_keepidle),
      0,
      INT_MAX,
      PGREST_CONF_SCALAR,
      pgrest_conf_keepalive_opt_set },

    { "tcp_keepintvl",
      offsetof(pgrest_conf_listener_t, tcp_keepintvl),
      0,
      INT_MAX,
      PGREST_CONF_SCALAR,
      pgrest_conf_keepalive_opt_set },

    { "tcp_keepcnt",
      offsetof(pgrest_conf_listener_t, tcp_keepcnt),
      0,
      INT_MAX,
      PGREST_CONF_SCALAR,
      pgrest_conf_keepalive_opt_set },

    { "ipv6_only",
      offsetof(pgrest_conf_listener_t, ipv6only),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_ipv6only_set },

    { "accept_filter",
      offsetof(pgrest_conf_listener_t, accept_filter),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_afilter_set },

    { "deferred_accept",
      offsetof(pgrest_conf_listener_t, deferred_accept),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_deferred_set },

    { "setfib",
      offsetof(pgrest_conf_listener_t, setfib),
      -1,
      INT_MAX,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set2 },

    { "fastopen",
      offsetof(pgrest_conf_listener_t, fastopen),
      -1,
      INT_MAX,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set2 },

    { "reuseport",
      offsetof(pgrest_conf_listener_t, reuseport),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_reuse_set },

    { "ssl",
      offsetof(pgrest_conf_listener_t, ssl),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_ssl_set },

    { "http2",
      offsetof(pgrest_conf_listener_t, http2),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_bool_set },

    { "server_name",
      offsetof(pgrest_conf_http_server_t, server_name),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_server_name_set },

#ifdef HAVE_OPENSSL
    { "ssl_certificate",
      offsetof(pgrest_conf_http_server_t, ssl_certificate),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_string_set },

    { "ssl_certificate_key",
      offsetof(pgrest_conf_http_server_t, ssl_certificate_key),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_string_set },

    { "ssl_session_cache",
      offsetof(pgrest_conf_http_server_t, ssl_session_cache),
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_bool_set },

    { "ssl_session_timeout",
      offsetof(pgrest_conf_http_server_t, ssl_session_timeout),
      60000,
      3600000,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set },
#endif

    { "client_header_timeout",
      offsetof(pgrest_conf_http_server_t, client_header_timeout),
      1000,
      3600000,
      PGREST_CONF_SCALAR,
      pgrest_conf_integer_set },

    { "location",
      0,
      0,
      0,
      PGREST_CONF_OBJECT,
      pgrest_conf_http_path_create },

    { "path",
      0,
      0,
      0,
      PGREST_CONF_SCALAR,
      pgrest_conf_http_path_set },

    { "handlers",
      0,
      0,
      0,
      PGREST_CONF_OBJECT,
      NULL },

    { NULL, 0, 0, 0, 0, NULL }
};
static List              *pgrest_conf_dynamic_cmds = NIL;
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

#ifdef HAVE_OPENSSL
static void
pgrest_conf_string_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    char  *newval = val;
    char  *object = parent;
    char **save;

    save = (char **) (object + cmd->offset);
    *save = pstrdup(newval);
}
#endif

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

    if (!pgrest_inet_parse_url(&u)) {
        if (u.err) {
            ereport(ERROR, 
                    (errmsg(PGREST_PACKAGE " " "invalid value \"%s\" for "
                            "directive \"%s\": \"%s\"", value, cmd->name, u.err)));
        }
    }

    memcpy(&conf_listener->sockaddr.sockaddr, &u.sockaddr, u.socklen);

    conf_listener->socklen = u.socklen;
    conf_listener->wildcard = u.wildcard;

    (void) pgrest_inet_ntop(&conf_listener->sockaddr.sockaddr, 
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
pgrest_conf_tbpath_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_setting_t *setting = parent;
    char             *value = val;
    char              buf[sizeof(setting->temp_buffer_path)];

    int len = snprintf(buf, sizeof(buf), "%s%s", value, 
                        strrchr(PGREST_BUFFER_DTEMP_PATH, '/'));
    if (len >= sizeof(buf)) {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value \"%s\" for "
                        "directive \"%s\": path too long", value, cmd->name)));
    }

    strcpy(setting->temp_buffer_path, buf);
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

static void  
pgrest_conf_http_path_set(pgrest_conf_command_t *cmd, void *val, void *parent)
{
    pgrest_conf_http_path_t   *conf_http_path = parent;
    char                      *value = val;
    pgrest_conf_http_server_t *conf_http_server;

    if (!value || value[0] == '\0') {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value for directive"
                         " \"%s\": path must be not null", cmd->name)));
    }

    /* need add conf_http_path to conf_http_server */
    conf_http_server = conf_http_path->conf_server;

    conf_http_path->path.base = (unsigned char *)pstrdup(value);
    conf_http_path->path.len = strlen(value);

    if (!pgrest_rtree_add(conf_http_server->paths, 
                          conf_http_path->path, 
                          conf_http_path)) 
    {
        ereport(ERROR, (errmsg(PGREST_PACKAGE " " "add path: \"%s\""
                               " to path router failed", value)));
    }
}

static void *
pgrest_conf_setting_create(void *parent)
{
    pgrest_setting_private->worker_processes = pgrest_util_ncpu();
    pgrest_setting_private->worker_priority = 0;
    pgrest_setting_private->worker_nofile = 1024;
    pgrest_setting_private->worker_noconn = 1024;
    pgrest_setting_private->acceptor_mutex = true;
    pgrest_setting_private->acceptor_mutex_delay = 500;
    pgrest_setting_private->acceptor_multi_accept = false;
    pgrest_setting_private->http_tcp_nopush = false;
    pgrest_setting_private->http_keepalive_timeout = 600000;
    strcpy(pgrest_setting_private->temp_buffer_path, 
            PGREST_BUFFER_DTEMP_PATH);

    pgrest_array_init(&pgrest_setting_private->conf_http_servers,
                      2, 
                      sizeof(pgrest_conf_http_server_t));
    pgrest_array_init(&pgrest_setting_private->conf_listeners, 
                      2, 
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
    conf_http_server->paths = pgrest_rtree_create();

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

    (void) pgrest_inet_ntop(&conf_listener->sockaddr.sockaddr, 
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

static void *
pgrest_conf_http_path_create(void *parent)
{
    pgrest_conf_http_path_t   *conf_http_path;

    conf_http_path = palloc0(sizeof(pgrest_conf_http_path_t));

    pgrest_array_init(&conf_http_path->handlers, 2, sizeof(void *));
    pgrest_array_init(&conf_http_path->filters, 2, sizeof(void *));
    conf_http_path->conf_server = parent;

    return conf_http_path;
}

static pgrest_conf_command_t *
pgrest_conf_cmd_find(const char *name)
{
    pgrest_conf_command_t  *cmd;
    ListCell               *cell;

    for ( cmd = pgrest_conf_static_cmds ; cmd->name; cmd++) {
        if (!strcmp(cmd->name, name)) {
            return cmd;
        }
    }

    foreach(cell, pgrest_conf_dynamic_cmds) {
        cmd = lfirst(cell);
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
    pgrest_conf_set_pt           handler;

    cmd = pgrest_conf_cmd_find(name);
    Assert(cmd->handler);

    if (!(cmd->flags & PGREST_CONF_SCALAR)) {
        ereport(ERROR, (errmsg(PGREST_PACKAGE " " "directive \"%s\""
                               " must be an object", cmd->name)));
    }

    handler = cmd->handler;
    handler(cmd, val, parent);
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
    pgrest_conf_create_pt   handler;

    cmd = pgrest_conf_cmd_find(name);
    if (!(cmd->flags & PGREST_CONF_OBJECT)) {
        ereport(ERROR, (errmsg(PGREST_PACKAGE " " "directive \"%s\""
                                " must be scalar", cmd->name)));
    }

    object = parent;
    if (cmd->handler) {
        handler = cmd->handler;
        object = handler(parent);
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
                       json_t **jroot)
{
    json_error_t            jerror;

    *jroot = json_load_file(filename, 0, &jerror);
    if (!*jroot) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "parse configure file: \"%s\" "
                                 "failed: %s line: %d", filename, jerror.text, 
                                  jerror.line)));
        return false;
    }

    return true;
}

static void
pgrest_conf_parse_fini(json_t *jroot)
{
    json_decref(jroot);
}

static bool
pgrest_conf_parse_iner(json_t *jroot)
{
    bool result = true;
    MemoryContext cctx = CurrentMemoryContext;

    PG_TRY();
    {
        pgrest_conf_parse_object("root", jroot, NULL);
    }
    PG_CATCH();
    {
        EmitErrorReport();
        FlushErrorState();
        MemoryContextSwitchTo(cctx);
        result = false;
    }
    PG_END_TRY();

    return result;
}

static void 
pgrest_conf_def_cmd_(const char *name, size_t offset, int min_val,
                     int max_val, int flags, void *handler)
{
    pgrest_conf_command_t *cmd;

    cmd = palloc0(sizeof(pgrest_conf_command_t));

    cmd->name = name;
    cmd->offset = offset;
    cmd->min_val = min_val;
    cmd->max_val = max_val;
    cmd->flags = flags;
    cmd->handler = handler;

    pgrest_conf_dynamic_cmds = lappend(pgrest_conf_dynamic_cmds, cmd);
}

void 
pgrest_conf_def_cmd(const char *name, size_t offset, int min_val,
                    int max_val, pgrest_conf_set_pt handler)
{
    pgrest_conf_def_cmd_(name, offset, min_val, max_val,
                         PGREST_CONF_SCALAR, handler);
}

void 
pgrest_conf_def_obj(const char *name, pgrest_conf_create_pt handler)
{
    pgrest_conf_def_cmd_(name, 0, 0, 0, PGREST_CONF_OBJECT, handler);
}

bool 
pgrest_conf_parse(pgrest_setting_t *setting, const char *filename)
{
    json_t                 *jroot;

    pgrest_setting_private = setting;
    
    if (!pgrest_conf_parse_init(filename, &jroot)) {
        return false;
    }

    if (!pgrest_conf_parse_iner(jroot)) {
        pgrest_conf_parse_fini(jroot);
        return false;
    }

    /* setup default values if need */
    pgrest_conf_default_values(pgrest_setting_private);

    if (!pgrest_conf_values_check(pgrest_setting_private)) {
        pgrest_conf_parse_fini(jroot);
        return false;
    }

    /* release resource */
    pgrest_conf_parse_fini(jroot);
    return true;
}
