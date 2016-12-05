/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_push_stream.h
 *
 * http push stream module.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_PUSH_STREAM_H
#define PG_REST_HTTP_PUSH_STREAM_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

typedef struct {
    pgrest_http_handler_t  super;
    void                  *conf;
} pgrest_http_push_stream_handler_t;

void pgrest_http_push_stream_conf_register(void);


#endif /* PG_REST_HTTP_PUSH_STREAM_H */
