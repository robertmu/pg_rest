/* -------------------------------------------------------------------------
 *
 * pq_rest_http_v1.c
 *   http v1 routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http_v1.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

static void
pgrest_http_entity_handler(evutil_socket_t fd, short events, void *arg);


static bool
pgrest_http_init_headers(pgrest_http_request_t *req, 
                         const struct phr_header *headers,
                         size_t num_headers)
{
    size_t                            i;
    pgrest_param_t                   *header;
    const pgrest_http_header_token_t *name;

    if (pgrest_array_init(&req->headers_in.headers, req->pool->mctx,
                                    10, sizeof(pgrest_param_t)) == false) {
        pgrest_http_close_request(req, PGREST_HTTP_INTERNAL_SERVER_ERROR);
        return false;
    }

    for (i = 0; i < num_headers; i++) {
        header = (pgrest_param_t *) &headers[i];
        pgrest_strtolower((char *)header->key.base, header->key.len);

        if (!pgrest_http_header_add(&req->headers_in.headers, &header)) {
            pgrest_http_close_request(req, PGREST_HTTP_INTERNAL_SERVER_ERROR);
            return false;
        }

        if ((name = pgrest_http_header_find_token((const char *)header->key.base, 
                                                  header->key.len)) != NULL) 
        {
            if (name->handler && !name->handler(req, header, name->offset)) {
                return false;
            }
        }
    }

    return true;
}

static bool 
pgrest_http_process_header(pgrest_http_request_t *req,
                           struct phr_header *headers,
                           size_t num_headers)
{
    if (!pgrest_http_init_headers(req, headers, num_headers)) {
        return false;
    }

    if (req->headers_in.server.len == 0
        && !pgrest_http_set_virtual_server(req, &req->headers_in.server)) {
        return false;
    }

    if (req->headers_in.host == NULL 
        && req->http_version > PGREST_HTTP_VERSION_10) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client sent HTTP/"
                             "1.1 request without \"Host\" header")));
        pgrest_http_finalize_request(req, PGREST_HTTP_BAD_REQUEST);
        return false;
    }

    if (req->headers_in.content_length) {
        req->headers_in.content_length_n =
                        pgrest_atoof(req->headers_in.content_length->value.base,
                                     req->headers_in.content_length->value.len);

        if (req->headers_in.content_length_n == PGREST_ERROR) {
            ereport(LOG, (errmsg(PGREST_PACKAGE " " "client sent in"
                                 "valid \"Content-Length\" header")));
            pgrest_http_finalize_request(req, PGREST_HTTP_BAD_REQUEST);
            return false;
        }
    }

    if (req->method == PGREST_HTTP_TRACE) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client sent TRACE method")));
        pgrest_http_finalize_request(req, PGREST_HTTP_NOT_ALLOWED);
        return false;
    }

    if (req->headers_in.transfer_encoding) {
        if (req->headers_in.transfer_encoding->value.len == 7
            && strncasecmp((const char *)req->headers_in.transfer_encoding->value.base,
                               "chunked", 7) == 0)
        {
            req->headers_in.content_length = NULL;
            req->headers_in.content_length_n = -1;
            req->headers_in.chunked = 1;

        } else if (req->headers_in.transfer_encoding->value.len != 8
            || strncasecmp((const char *)req->headers_in.transfer_encoding->value.base,
                               "identity", 8) != 0)
        {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "client sent un"
                            "known\"Transfer-Encoding\": \"%s\"", 
                     req->headers_in.transfer_encoding->value.base)));
            pgrest_http_finalize_request(req, PGREST_HTTP_NOT_IMPLEMENTED);
            return false;
        }
    }

    if (req->headers_in.connection_type == PGREST_HTTP_CONNECTION_KEEP_ALIVE) {
        if (req->headers_in.keep_alive) {
            req->headers_in.keep_alive_n =
                            pgrest_atotm(req->headers_in.keep_alive->value.base,
                                         req->headers_in.keep_alive->value.len);
        }
    }

    return true;
}

static ssize_t
pgrest_http_read(pgrest_http_request_t *req, pgrest_buffer_t *buf, int timeout)
{
    ssize_t                    n;
    pgrest_connection_t       *conn = req->conn;
    pgrest_http_connection_t  *hconn = req->http_conn;

    if (hconn->parse) {
        hconn->parse = 0;
        return buf->size;
    }

    if (conn->ready) {
        n = pgrest_conn_recv(conn, buf->pos + buf->size, 
                             pgrest_buffer_free_size(buf));
    } else {
        n = PGREST_AGAIN;
    }

    if (n == PGREST_AGAIN) {
        if (!pgrest_event_add(conn->rev, EV_READ, timeout)) {
            pgrest_http_close_request(req, PGREST_HTTP_INTERNAL_SERVER_ERROR);
            return PGREST_ERROR;
        }

        return PGREST_AGAIN;
    }

    if (n == 0) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client pre"
                             "maturely closed connection")));
    }

    if (n == 0 || n == PGREST_ERROR) {
        conn->error = 1;
        pgrest_http_finalize_request(req, PGREST_HTTP_BAD_REQUEST);
        return PGREST_ERROR;
    }

    buf->size += n;

    return n;
}

static int
pgrest_http_body_chunked(pgrest_http_request_t *req, pgrest_buffer_t *buf)
{
    ssize_t                     n;
    pgrest_http_request_body_t *rb;
    pgrest_conf_http_server_t  *conf;
    size_t                      size;

    conf = req->http_conf;
    rb = req->request_body;

    if (rb->rest == -1) {
        debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http req"
                                  "uest body chunked parser")));

        rb->chunked = pgrest_mpool_calloc(req->pool, 1, 
                                        sizeof(struct phr_chunked_decoder));
        if (rb->chunked == NULL) {
            pgrest_http_finalize_request(req, PGREST_HTTP_INTERNAL_SERVER_ERROR);
            return PGREST_ERROR;
        }

        rb->rest = 3;
        rb->chunked->consume_trailer = 1;
        rb->decode_length = rb->split ? 0 : req->request_length;

        req->headers_in.content_length_n = 0;
    }

    /* decode the incoming data */
    if ((size = buf->size - rb->decode_length) == 0) {
        return PGREST_AGAIN;
    }

    n = phr_decode_chunked(rb->chunked, (char *)buf->pos + rb->decode_length, &size);
    if (n == PGREST_ERROR) {
        /* error */
        pgrest_http_finalize_request(req, PGREST_HTTP_BAD_REQUEST);
        return PGREST_ERROR;
    }

    rb->decode_length += size;
    buf->size = rb->decode_length;

    if (rb->split ? buf->size : buf->size - req->request_length 
            >= conf->client_max_body_size) 
    {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client intended"
                             " to send too large chunked body")));

        pgrest_http_finalize_request(req, PGREST_HTTP_REQUEST_ENTITY_TOO_LARGE);
        return PGREST_ERROR;
    }

    if (n == PGREST_AGAIN) {
        /* incomplete */
        return PGREST_AGAIN;
    }

    /* complete */
    rb->rest = 0;
    rb->entity.base = rb->split ? buf->pos : buf->pos + req->request_length;
    rb->entity.len = rb->split ? buf->size : buf->size - req->request_length;

    req->headers_in.content_length_n = rb->entity.len;
    req->request_length += rb->entity.len;

    buf->size += n;

    return PGREST_OK;
}

static int
pgrest_http_body_length(pgrest_http_request_t *req, pgrest_buffer_t *buf)
{
    pgrest_http_request_body_t *rb;
    pgrest_conf_http_server_t  *conf;
    size_t                      size;

    conf = req->http_conf;
    rb = req->request_body;

    size = rb->split ? buf->size : buf->size - req->request_length;

    if (conf->client_max_body_size < size) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client intended to"
                             " send too large body: " INT64_FORMAT,
                              req->headers_in.content_length_n)));

        pgrest_http_finalize_request(req, PGREST_HTTP_REQUEST_ENTITY_TOO_LARGE);
        return PGREST_ERROR;
    }

    if (rb->rest == -1) {
        debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http request"
                                  " body content length parser")));
        rb->rest = req->headers_in.content_length_n;
    }

    if ((off_t) size < rb->rest) {
        return PGREST_AGAIN;
    }

    rb->rest = 0;

    rb->entity.base = rb->split ? buf->pos : buf->pos + req->request_length;
    rb->entity.len = req->headers_in.content_length_n;

    req->request_length += rb->entity.len;

    return PGREST_OK;
}

static int
pgrest_http_body_parser(pgrest_http_request_t *req, pgrest_buffer_t *buf)
{
    if (req->headers_in.chunked) {
        return pgrest_http_body_chunked(req, buf);
    } else {
        return pgrest_http_body_length(req, buf);
    }
}

static void 
pgrest_http_process_entity(pgrest_http_request_t *req)
{
    pgrest_http_request_body_t *rb;
    int                         rc;
    pgrest_conf_http_server_t  *conf;
    pgrest_connection_t        *conn;
    size_t                      preread;
    size_t                      left = 0;

    conn = req->conn;
    conf = req->http_conf;

    rb = pgrest_mpool_calloc(req->pool, 1, sizeof(pgrest_http_request_body_t));
    if (rb == NULL) {
        pgrest_http_finalize_request(req, PGREST_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    rb->rest = -1;
    req->request_body = rb;

    preread = req->header_in->size - req->request_length;
    if (preread) {
        /* there is the pre-read part of the request body */
        debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http client req"
                                  "uest body preread %zu", preread)));

        rc = pgrest_http_body_parser(req, req->header_in);
        switch (rc) {
        case PGREST_OK:
            pgrest_http_process_request(req);
        case PGREST_ERROR:
            return;
        default:
            if (!req->headers_in.chunked) {
                left = req->headers_in.content_length_n - preread;
            }
        }
    } else {
        if (!req->headers_in.chunked) {
            left = req->headers_in.content_length_n;
        }
    }

    /* prepare body buffer */
    if (left && left <= pgrest_buffer_free_size(req->header_in)) {
        rb->buffer = req->header_in;
        rb->split = 0;
    } else {
        rb->buffer = pgrest_buffer_create(req->pool, 
                                          conf->client_body_buffer_size);
        if (rb->buffer == NULL) {
            pgrest_http_finalize_request(req, 
                                         PGREST_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        /* copy partial body to body buffer */
        if (preread) {
            pgrest_iovec_t  iovec;

            iovec = pgrest_buffer_reserve(req->pool, 
                                          &rb->buffer,
                                          preread);

            if (iovec.base == NULL) {
                pgrest_http_finalize_request(req, 
                                         PGREST_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            memcpy(rb->buffer->pos, 
                   req->header_in->pos + req->request_length, 
                   preread);
            rb->buffer->size = preread;

            req->header_in->size = req->request_length;
        }

        rb->split = 1;
    }

    conn->rev_handler = pgrest_http_entity_handler;
    pgrest_http_entity_handler(conn->fd, EV_READ, conn);
}

static void
pgrest_http_entity_handler(evutil_socket_t fd, short events, void *arg)
{
    ssize_t                     n;
    pgrest_buffer_t            *buf;
    pgrest_http_request_t      *req;
    pgrest_connection_t        *conn;
    pgrest_conf_http_server_t  *conf;
    int                         result;

    conn = (pgrest_connection_t *) arg;
    req = conn->data;
    conf = req->http_conf;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http entity handler")));

    if (events & EV_TIMEOUT) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client body time out")));
        conn->timedout = 1;
        pgrest_http_finalize_request(req, PGREST_HTTP_REQUEST_TIME_OUT);
        return;
    }

    buf = req->request_body->buffer;

    result = PGREST_AGAIN;

    for ( ;; ) {

        if (result == PGREST_AGAIN) {

            n = pgrest_http_read(req, 
                                 req->request_body->buffer, 
                                 conf->client_body_timeout);

            if (n == PGREST_AGAIN || n == PGREST_ERROR) {
                return;
            }
        }

        result = pgrest_http_body_parser(req, req->request_body->buffer);
        switch (result) {
        case PGREST_OK:
            pgrest_http_process_request(req);
        case PGREST_ERROR:
            return;
        default:
            if (buf->size == buf->capacity) {
                if (pgrest_http_extend_body_buffer(req) == PGREST_ERROR) {
                    pgrest_http_finalize_request(req, 
                                            PGREST_HTTP_INTERNAL_SERVER_ERROR);
                    return;
                }
            }
            continue;
        }
    }
}

void
pgrest_http_header_handler(evutil_socket_t fd, short events, void *arg)
{
    ssize_t                   n;
    pgrest_http_request_t    *req;
    pgrest_connection_t      *conn;
    pgrest_http_connection_t *hconn;
    pgrest_string_t           method; /* XXX */
    int                       rv, result, minor_version;
    struct phr_header         headers[PGREST_HTTP_MAX_HEADERS];
    size_t                    num_headers = PGREST_HTTP_MAX_HEADERS;
    static pgrest_string_t    http2 = pgrest_string("PRI * HTTP/2");

    conn = (pgrest_connection_t *) arg;
    req = conn->data;
    hconn = req->http_conn;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http header handler")));

    if (events & EV_TIMEOUT) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client header time out")));
        conn->timedout = 1;
        pgrest_http_close_request(req, PGREST_HTTP_REQUEST_TIME_OUT);
        return;
    }

    result = PGREST_AGAIN;

    for ( ;; ) {
        if (result == PGREST_AGAIN) {

            n = pgrest_http_read(req, 
                                 req->header_in, 
                                 conn->listener->post_accept_timeout);
            if (n == PGREST_AGAIN || n == PGREST_ERROR) {
                return;
            }
        }

        result = phr_parse_request((const char *)req->header_in->pos, 
                                   req->header_in->size, 
                                   (const char **) &method.base,
                                   &method.len, 
                                   (const char **) &req->path.base, 
                                   &req->path.len,
                                   &minor_version, 
                                   headers, 
                                   &num_headers, 
                                   hconn->prev_size);
        hconn->prev_size = req->header_in->size;

        switch (result) {
        default: /* parse complete */
            req->request_length = result;
            req->http_version = 0x100 | (minor_version != 0);

            if (!pgrest_http_process_header(req, headers, num_headers)) {
                return;
            }

            if (req->headers_in.content_length_n != -1 
                || req->headers_in.chunked) 
            {
                if (!pgrest_http_test_expect(req)) {
                    return;
                }

                pgrest_http_process_entity(req);
            } else {
                req->header_only = 1;
                pgrest_http_process_request(req);
            }

            return;

        case PGREST_AGAIN: /* incomplete */
            if (pgrest_buffer_free_size(req->header_in) < 1) {
                rv = pgrest_http_extend_header_buffer(req);
                if (rv == PGREST_ERROR) {
                    pgrest_http_close_request(req, 
                            PGREST_HTTP_INTERNAL_SERVER_ERROR);
                    return;
                }

                if (rv == PGREST_DECLINED) {
                    req->lingering_close = 1;
                    ereport(LOG, (errmsg(PGREST_PACKAGE " " "client sent"
                                          " too large request header")));
                    pgrest_http_finalize_request(req,
                            PGREST_HTTP_REQUEST_HEADER_TOO_LARGE);
                    return;
                }
            }

            continue;

        case PGREST_ERROR: /* error */
            if (req->header_in->size >= http2.len
                && memcmp(req->header_in->pos, http2.base, http2.len) == 0) {

                /* upgrade to http2 */
                pgrest_http_v2_conn_init(conn);
                return;
            }

            pgrest_http_finalize_request(req, PGREST_HTTP_BAD_REQUEST);
            return;
        }
    }
}

void
pgrest_http_init_handler(evutil_socket_t fd, short events, void *arg)
{
    size_t                     size;
    ssize_t                    n;
    pgrest_connection_t       *conn;
    pgrest_http_connection_t  *hconn;
    pgrest_conf_http_server_t *conf;
    int                        post_accept_timeout;

    conn = (pgrest_connection_t *) arg;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http init handler")));

    if (events & EV_TIMEOUT) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client time out")));
        pgrest_http_conn_close(conn);
        return;
    }

    if (conn->close) {
        pgrest_http_conn_close(conn);
        return;
    }

    hconn = conn->data;
    conf = hconn->addr_conf->default_server;

    size = conf->client_header_buffer_size;

    if (conn->buffer == NULL) {
        conn->buffer = pgrest_buffer_create(conn->pool, size);
        if (conn->buffer == NULL) {
            pgrest_http_conn_close(conn);
            return;
        }
    } 

    n = pgrest_conn_recv(conn, conn->buffer->pos, size);
    if (n == PGREST_AGAIN) {
        post_accept_timeout = conn->listener->post_accept_timeout;
        if (!pgrest_event_add(conn->rev, EV_READ, post_accept_timeout)) {
            pgrest_http_conn_close(conn);
        }

        pgrest_conn_reusable(conn, 1);
        return;
    }

    if (n == PGREST_ERROR) {
        pgrest_http_conn_close(conn);
        return;
    }

    if (n == 0) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client closed connection")));
        pgrest_http_conn_close(conn);
        return;
    }

    conn->buffer->size += n;

    pgrest_conn_reusable(conn, 0);

    conn->data = pgrest_http_create_request(conn);
    if (conn->data == NULL) {
        pgrest_http_conn_close(conn);
        return;
    }

    hconn->parse = 1;
    conn->rev_handler = pgrest_http_header_handler;
    pgrest_http_header_handler(conn->fd, EV_READ, conn);
}
