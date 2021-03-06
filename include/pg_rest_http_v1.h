/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_v1.h
 *
 * http v1 handler.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_V1_H
#define PG_REST_HTTP_V1_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

void
pgrest_http_init_handler(evutil_socket_t fd, short events, void *arg);
void
pgrest_http_header_handler(evutil_socket_t fd, short events, void *arg);


#endif /* PG_REST_HTTP_V1_H */
