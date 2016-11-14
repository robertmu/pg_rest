/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_request.h
 *
 * http request handler.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_REQUEST_H
#define PG_REST_HTTP_REQUEST_H

/**
 * HTTP request
 */
struct pgrest_http_request_s {
    /* the underlying connection */
    pgrest_connection_t *conn;
    /* the HTTP version (represented as 0xMMmm (M=major, m=minor)) */
    int version;
};

#endif /* PG_REST_HTTP_REQUEST_H */
