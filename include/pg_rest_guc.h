/*-------------------------------------------------------------------------
 *
 * include/pg_rest_guc.h
 *
 * guc settings.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_GUC_H
#define PG_REST_GUC_H

typedef struct pgrest_setting_s  pgrest_setting_t;

struct pgrest_setting_s {
    char *listener_addresses;
    int   listener_backlog;
    int   listener_tcp_rcvbuf;
    int   listener_tcp_sndbuf;
    char *listener_tcp_keepalive;
    char *listener_unix_socket_dirs;
    int   listener_tcp_keepintvl;
    int   listener_tcp_keepidle;
    int   listener_tcp_keepcnt;
    bool  listener_ipv6only;

    bool  http_enabled;
    int   http_port;
    char *crud_url_prefix;
    char *doc_url_prefix;
    char *sql_url_prefix;
    bool  https_enabled;
    int   https_port;
    char *https_ssl_key;

    char *acceptor_accept_filter;
    bool  acceptor_deferred_accept;
    int   acceptor_setfib;
    int   acceptor_fastopen;
    bool  acceptor_reuseport;
    int   acceptor_timeout;
    bool  acceptor_mutex;
    int   acceptor_mutex_delay;
    bool  acceptor_multi_accept;

    int   worker_processes;
    int   worker_priority;
    int   worker_noconn;
    int   worker_nofile;
};

extern pgrest_setting_t pgrest_setting;

void pgrest_guc_init(char *setting);
void pgrest_guc_info_print(char *setting);

#endif /* PG_REST_GUC_H */
