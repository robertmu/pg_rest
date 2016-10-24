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

typedef struct pgrest_conf_command_s     pgrest_conf_command_t;
typedef void  *(*pgrest_conf_create_pt)  (void *);
typedef void   (*pgrest_conf_set_pt)     (pgrest_conf_command_t *, 
                                          void *, 
                                          void *);
typedef struct pgrest_conf_http_server_s pgrest_conf_http_server_t;

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
    char                       name[NAMEDATALEN + 1];
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
    unsigned                   listen:1;
    void                      *conf_main;
};

void pgrest_conf_def_cmd(const char           *name,
                         size_t                offset,
                         int                   min_val,
                         int                   max_val,
                         pgrest_conf_create_pt create_conf,
                         pgrest_conf_set_pt    set);

bool pgrest_conf_parse(pgrest_setting_t *setting, const char *filename);

#endif /* PG_REST_CONF_H */
