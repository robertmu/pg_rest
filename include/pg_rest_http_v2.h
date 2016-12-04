/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_v2.h
 *
 * http v2 handler.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_V2_H
#define PG_REST_HTTP_V2_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

void
pgrest_http_v2_init(evutil_socket_t fd, short events, void *arg);
void
pgrest_http_v2_conn_init(pgrest_connection_t *conn);

#endif /* PG_REST_HTTP_V2_H */
