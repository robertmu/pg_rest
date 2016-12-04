/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_upstream.h
 *
 * http upstream routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_UPSTREAM_H
#define PG_REST_HTTP_UPSTREAM_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

#define PGREST_HTTP_UPSTREAM_CREATE            0x0001

typedef struct pgrest_http_upstream_srv_conf_s pgrest_http_upstream_srv_conf_t;

typedef int (*pgrest_http_upstream_init_pt) (pgrest_http_upstream_srv_conf_t *);
typedef int (*pgrest_http_upstream_initp_pt) (pgrest_http_request_t *,
                                             pgrest_http_upstream_srv_conf_t *);

typedef enum {
    PGREST_HTTP_UPSTREAM_RWSPLIT = 0,
    PGREST_HTTP_UPSTREAM_SHARDING,
    PGREST_HTTP_UPSTREAM_UNKNOW
} pgrest_http_upstream_rpolicy_e;

typedef struct {
    pgrest_http_upstream_init_pt     init_upstream;
    pgrest_http_upstream_initp_pt    init;
    void                            *data;
} pgrest_http_upstream_peer_t;

typedef struct {
    pgrest_string_t                  name;
    pgrest_addr_t                   *addrs;
    pgrest_uint_t                    naddrs;
} pgrest_http_upstream_server_t;

struct pgrest_http_upstream_srv_conf_s {
    pgrest_http_upstream_peer_t      peer;
    pgrest_http_upstream_rpolicy_e   policy;
    pgrest_array_t                  *servers;
    pgrest_uint_t                    flags;
    pgrest_string_t                  host;
    in_port_t                        port;
    in_port_t                        default_port;
    pgrest_uint_t                    no_port;
};

typedef struct {
    pgrest_http_upstream_srv_conf_t *upstream;
    size_t                           buffer_size;
} pgrest_http_upstream_conf_t;

struct pgrest_http_upstream_s {
    int   (*create_request)(pgrest_http_request_t *req);
    int   (*reinit_request)(pgrest_http_request_t *req);
    int   (*process_header)(pgrest_http_request_t *req);
    void  (*abort_request)(pgrest_http_request_t *req);
    void  (*finalize_request)(pgrest_http_request_t *req, int rc);
};


bool
pgrest_http_upstream_add(pgrest_url_t *u, 
                         pgrest_uint_t flags, 
                         pgrest_http_upstream_srv_conf_t **conf);
void pgrest_http_upstream_conf_register(void);
bool pgrest_http_upstream_init(void);

#endif /* PG_REST_HTTP_UPSTREAM_H */
