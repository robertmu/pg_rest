/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http.h
 *
 * http common routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_H
#define PG_REST_HTTP_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct   pgrest_http_request_s    pgrest_http_request_t;
typedef struct   pgrest_http_upstream_s   pgrest_http_upstream_t;

typedef bool (*pgrest_http_header_handler_pt)(pgrest_http_request_t *,
    pgrest_param_t *, size_t);

#include "pg_rest_http_core.h"
#include "pg_rest_http_request.h"
#include "pg_rest_http_header.h"
#include "pg_rest_http_upstream.h"
#include "picohttpparser.h"
#include "pg_rest_http_v1.h"
#include "pg_rest_http_v2.h"

void pgrest_http_conn_init(pgrest_connection_t *conn);
void pgrest_http_conn_close(pgrest_connection_t *conn);
void pgrest_http_init(pgrest_setting_t *setting);
void pgrest_http_setup_configurators(void);
pgrest_http_request_t *pgrest_http_create_request(pgrest_connection_t *conn);
int  pgrest_http_extend_header_buffer(pgrest_http_request_t *req);

#endif /* PG_REST_HTTP_H */
