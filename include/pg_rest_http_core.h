/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_core.h
 *
 * http path filter/handler routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_CORE_H
#define PG_REST_HTTP_CORE_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

typedef struct   pgrest_http_handler_s        pgrest_http_handler_t;
typedef struct   pgrest_http_filter_s         pgrest_http_filter_t;
typedef struct   pgrest_http_ostream_s        pgrest_http_ostream_t;
typedef int    (*pgrest_http_handler_pt)      (pgrest_http_handler_t  *self,
                                              pgrest_http_request_t  *req);
typedef void   (*pgrest_http_filter_pt)       (pgrest_http_filter_t   *self,
                                              pgrest_http_request_t  *req,
                                              pgrest_http_ostream_t **slot);
typedef bool   (*pgrest_http_handler_init_pt) (pgrest_http_handler_t *self);
typedef void   (*pgrest_http_handler_fini_pt) (pgrest_http_handler_t *self);


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
    pgrest_array_t               addrs;    /* array of pgrest_http_conf_addr_t */
} pgrest_http_conf_port_t;

struct pgrest_http_ostream_s {
    pgrest_http_ostream_t       *next;
};

struct pgrest_http_handler_s {
    const char                  *name;
    pgrest_http_handler_init_pt  init;
    pgrest_http_handler_fini_pt  fini;
    pgrest_http_handler_pt       handler;
};

struct pgrest_http_filter_s {
    pgrest_http_filter_pt        handler;
};

pgrest_http_handler_t *
pgrest_http_handler_create(pgrest_conf_http_path_t *path, 
                           size_t size);
pgrest_http_filter_t *
pgrest_http_filter_create(pgrest_conf_http_path_t *path, 
                          size_t size);

#endif /* PG_REST_HTTP_CORE_H */
