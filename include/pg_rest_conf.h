/*-------------------------------------------------------------------------
 *
 * include/pg_rest_conf.h
 *
 * pg_rest configure option.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_CONF_H
#define PG_REST_CONF_H

#define PGREST_CONF_OBJECT               0x0001
#define PGREST_CONF_SCALAR               0x0002
#define PGREST_HTTP_LINGERING_OFF        0
#define PGREST_HTTP_LINGERING_ON         1
#define PGREST_HTTP_LINGERING_ALWAYS     2

typedef struct pgrest_conf_command_s     pgrest_conf_command_t;
typedef void  *(*pgrest_conf_create_pt)  (void *);
typedef void   (*pgrest_conf_set_pt)     (pgrest_conf_command_t *, 
                                          void *, 
                                          void *);
typedef struct pgrest_conf_http_server_s pgrest_conf_http_server_t;

struct pgrest_conf_command_s {
    const char                *name;
    size_t                     offset;
    int                        min_val;
    int                        max_val;
    int                        flags;
    void                      *handler;
};

typedef struct {
    pgrest_sockaddr_t          sockaddr;
    socklen_t                  socklen;

    bool                       set;
    bool                       default_server;
    bool                       bind;
    bool                       wildcard;
    bool                       ssl;
    bool                       http2;
    bool                       ipv6only;
    bool                       reuseport;
    int                        so_keepalive;
    int                        backlog;
    int                        rcvbuf;
    int                        sndbuf;
    int                        setfib;
    int                        fastopen;
    int                        tcp_keepidle;
    int                        tcp_keepintvl;
    int                        tcp_keepcnt;
    char                       accept_filter[PGREST_AF_SIZE];
    bool                       deferred_accept;
    char                       addr[PGREST_SOCKADDR_STRLEN];
} pgrest_conf_listener_t;

typedef struct {
    char                       name[NAMEDATALEN];
    pgrest_conf_http_server_t *server;   /* virtual name server conf */
} pgrest_http_server_name_t;

struct pgrest_conf_http_server_s {
    char                      *server_name;
#ifdef HAVE_OPENSSL
    char                      *ssl_certificate;
    char                      *ssl_certificate_key;
    bool                       ssl_session_cache;
    int                        ssl_session_timeout;
#endif
    bool                       tcp_nopush;
    bool                       tcp_nodelay;
    int                        keepalive_timeout;
    int                        client_header_timeout;
    int                        client_header_buffer_size;
    int                        client_max_header_size;
    int                        client_max_body_size;
    int                        client_body_timeout;
    int                        client_body_buffer_size;
    int                        lingering_time;
    int                        lingering_timeout;
    int                        lingering_close;
    bool                       reset_timedout_connection;
    pgrest_array_t             server_names;
    pgrest_rtree_node_t       *paths;
    void                      *conf_main;
    unsigned                   listen:1;
};

typedef struct {
    pgrest_string_t            path;
    pgrest_array_t             handlers;
    pgrest_array_t             filters;
    void                      *conf_server;
} pgrest_conf_http_path_t;

void pgrest_conf_def_cmd(const char           *name,
                         size_t                offset,
                         int                   min_val,
                         int                   max_val,
                         pgrest_conf_set_pt    handler);
void pgrest_conf_def_obj(const char           *name,
                         pgrest_conf_create_pt handler);

bool pgrest_conf_parse(pgrest_setting_t *setting, const char *filename);

#endif /* PG_REST_CONF_H */
