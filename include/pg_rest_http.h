/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http.h
 *
 * http handler.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_H
#define PG_REST_HTTP_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct {
    HTAB                        *hash;
} pgrest_http_virtual_names_t;

typedef struct {
    /* the default server configuration for this address:port */
    pgrest_conf_http_server_t   *default_server;
    pgrest_http_virtual_names_t *virtual_names;
#ifdef HAVE_OPENSSL
    unsigned                     ssl:1;
#endif
    unsigned                     http2:1;
} pgrest_http_addr_conf_t;

typedef struct {
    in_addr_t                    addr;
    pgrest_http_addr_conf_t      conf;
} pgrest_http_in_addr_t;

#ifdef HAVE_IPV6
typedef struct {
    struct in6_addr              addr6;
    pgrest_http_addr_conf_t      conf;
} pgrest_http_in6_addr_t;
#endif

typedef struct {
    /* pgrest_http_in_addr_t or pgrest_http_in6_addr_t */
    void                        *addrs;
    pgrest_uint_t                naddrs;
} pgrest_http_port_t;

typedef struct {
    pgrest_conf_listener_t       opt;
    HTAB                        *hash;
    pgrest_conf_http_server_t   *default_server;
    pgrest_array_t               servers;  /* array of pgrest_conf_http_server_t */
} pgrest_http_conf_addr_t;

typedef struct {
    int                          family;
    in_port_t                    port;
    pgrest_array_t               addrs;     /* array of pgrest_http_conf_addr_t */
} pgrest_http_conf_port_t;

void pgrest_http_init(pgrest_setting_t *setting);

#endif /* PG_REST_HTTP_H */
