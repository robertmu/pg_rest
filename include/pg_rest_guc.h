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
    char           *configure_file;
    int             worker_processes;
    int             worker_priority;
    int             worker_noconn;
    int             worker_nofile;
    bool            acceptor_mutex;
    int             acceptor_mutex_delay;
    bool            acceptor_multi_accept;
    bool            http_tcp_nopush;
    int             http_keepalive_timeout;
    char            temp_buffer_path[MAXPGPATH];
    pgrest_array_t  conf_http_servers;       /* pgrest_conf_http_server_t */
    pgrest_array_t  conf_listeners;
    pgrest_array_t *conf_ports;
};

extern pgrest_setting_t pgrest_setting;

void pgrest_guc_init(char *setting);
void pgrest_guc_info_print(char *setting);

#endif /* PG_REST_GUC_H */
