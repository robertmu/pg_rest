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

#include "pg_rest_http_core.h"
#include "pg_rest_http_request.h"
#include "pg_rest_http_v1.h"

void pgrest_http_conn_init(pgrest_connection_t *conn);
void pgrest_http_conn_close(pgrest_connection_t *conn);
void pgrest_http_init(pgrest_setting_t *setting);

#endif /* PG_REST_HTTP_H */
