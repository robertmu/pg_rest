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

void pgrest_http_init(void);
void pgrest_http_conn_init(pgrest_connection_t *c);

#endif /* PG_REST_HTTP_H */
