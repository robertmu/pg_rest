/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_postgres.h
 *
 * http postgres module.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_POSTGRES_H
#define PG_REST_HTTP_POSTGRES_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

typedef struct {
    pgrest_http_handler_t  super;
    void                  *conf;
} pgrest_http_postgres_handler_t;

void pgrest_http_postgres_conf_register(void);

#endif /* PG_REST_HTTP_POSTGRES_H */
