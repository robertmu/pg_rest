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
    int                        client_header_timeout;
    pgrest_array_t             server_names;
    pgrest_rtree_node_t       *paths;
    unsigned                   listen:1;
    void                      *conf_main;
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
