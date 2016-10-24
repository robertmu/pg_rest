/* -------------------------------------------------------------------------
 *
 * pq_rest_http_request.c
 *   http request routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http_request.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

void 
pgrest_http_conn_init(pgrest_connection_t *conn)
{

}

void
pgrest_http_conn_close(pgrest_connection_t *conn)
{
    pgrest_mpool_t *pool;

#ifdef HAVE_OPENSSL
    /* TODO ssl  */
#endif

    conn->destroyed = 1;

    pool = conn->pool;
    pgrest_conn_close(conn);

    pgrest_mpool_destroy(pool);
}
