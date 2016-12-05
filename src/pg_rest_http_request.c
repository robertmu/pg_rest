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

static void
pgrest_http_free_request(pgrest_http_request_t *req, pgrest_int_t rc);


static inline void
pgrest_http_read_handler(evutil_socket_t fd, short events, void *arg)
{
    pgrest_connection_t *conn = (pgrest_connection_t *) arg;

    conn->ready = 1;
    conn->rev_handler(fd, events, arg);
}

static inline void
pgrest_http_write_handler(evutil_socket_t fd, short events, void *arg)
{
    pgrest_connection_t *conn = (pgrest_connection_t *) arg;
    conn->wev_handler(fd, events, arg);
}

static inline void
pgrest_http_empty_handler(evutil_socket_t fd, short events, void *arg)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http empty handler")));
}

void 
pgrest_http_conn_init(pgrest_connection_t *conn)
{
    pgrest_uint_t             i;
    struct sockaddr_in       *sin;
    pgrest_http_port_t       *port;
    pgrest_http_in_addr_t    *addr;
    pgrest_http_connection_t *hconn;
    struct event_base        *base;
#ifdef HAVE_IPV6
    struct sockaddr_in6      *sin6;
    pgrest_http_in6_addr_t   *addr6;
#endif
    int                       post_accept_timeout;

    if((hconn = pgrest_mpool_calloc(conn->pool, 1,
                                sizeof(pgrest_http_connection_t))) == NULL) {
        pgrest_http_conn_close(conn);
        return;
    }

    conn->data = hconn;
    base = pgrest_event_get_base(conn->rev);

    /* find the server configuration for the address:port */
    port = conn->listener->servers;
    if (port->naddrs > 1) {
        /*
         * there are several addresses on this port and one
         * of them is an "*:port" wildcard so getsockname()
         * is required to determine a server address
         */
        if (!pgrest_conn_local_sockaddr(conn, NULL, false)) {
            pgrest_http_conn_close(conn);
            return;
        }

        switch (conn->local_sockaddr->sa_family) {
#ifdef HAVE_IPV6
        case AF_INET6:
            sin6 = (struct sockaddr_in6 *) conn->local_sockaddr;

            addr6 = port->addrs;

            /* the last address is "*" */
            for (i = 0; i < port->naddrs - 1; i++) {
                if (memcmp(&addr6[i].addr6, &sin6->sin6_addr, 16) == 0) {
                    break;
                }
            }

            hconn->addr_conf = &addr6[i].conf;
            break;
#endif
        default: /* AF_INET */
            sin = (struct sockaddr_in *) conn->local_sockaddr;

            addr = port->addrs;

            /* the last address is "*" */
            for (i = 0; i < port->naddrs - 1; i++) {
                if (addr[i].addr == sin->sin_addr.s_addr) {
                    break;
                }
            }

            hconn->addr_conf = &addr[i].conf;
            break;
        }
    } else {
        switch (conn->local_sockaddr->sa_family) {
#ifdef HAVE_IPV6
        case AF_INET6:
            addr6 = port->addrs;
            hconn->addr_conf = &addr6[0].conf;
            break;
#endif
        default: /* AF_INET */
            addr = port->addrs;
            hconn->addr_conf = &addr[0].conf;
            break;
        }
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "client addr:%s server addr:"
                              "%s", conn->addr_text,conn->listener->addr_text)));

    conn->rev_handler = pgrest_http_init_handler;
    conn->wev_handler = pgrest_http_empty_handler;

    if (hconn->addr_conf->http2) {
        conn->rev_handler = pgrest_http_v2_init;
    }

#ifdef HAVE_OPENSSL
    /* TODO ssl */
#endif

#ifdef HAVE_DEFERRED_ACCEPT
    if (conn->listener->deferred_accept) {
        pgrest_http_read_handler(conn->fd, EV_READ, conn);
        return;
    }
#endif

    post_accept_timeout = conn->listener->post_accept_timeout;
    pgrest_conn_reusable(conn, 1);

    if (pgrest_event_assign(conn->rev, base, conn->fd, EV_READ,
                            pgrest_http_read_handler, conn) < 0) {
        pgrest_http_conn_close(conn);
        return;
    }

    if (pgrest_event_assign(conn->wev, base, conn->fd, EV_WRITE,
                            pgrest_http_write_handler, conn) < 0) {
        pgrest_http_conn_close(conn);
        return;
    }

    pgrest_event_priority_set(conn->rev, PGREST_EVENT_PRIORITY);
    pgrest_event_priority_set(conn->wev, PGREST_EVENT_PRIORITY);

    if (!pgrest_event_add(conn->rev, EV_READ, post_accept_timeout)) {
        pgrest_http_conn_close(conn);
    }
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

pgrest_http_request_t *
pgrest_http_create_request(pgrest_connection_t *conn)
{
    pgrest_mpool_t             *pool;
    pgrest_http_request_t      *req;
    pgrest_http_connection_t   *hconn;
    struct event_base          *base;

    hconn = conn->data;

    if ((pool = pgrest_mpool_create(conn->pool)) == NULL) {
        return NULL;
    }

    req = pgrest_mpool_calloc(pool, 1, sizeof(pgrest_http_request_t));
    if (req == NULL) {
        pgrest_mpool_destroy(pool);
        return NULL;
    }

    req->pool = pool;

    req->http_conn = hconn;
    req->conn = conn;

    req->read_event_handler = pgrest_http_block_reading;
    req->header_in = conn->buffer;

    if (pgrest_array_init(&req->headers_out.headers, req->pool->mctx,
                                    10, sizeof(pgrest_param_t)) == false) {
        pgrest_mpool_destroy(req->pool);
        return NULL;
    }

#ifdef HAVE_OPENSSL
    /* TODO ssl */
#endif

    req->count = 1;

    base = pgrest_event_get_base(conn->rev);
    (void) pgrest_event_gettimeofday(base, &req->start_time);

    req->method = PGREST_HTTP_UNKNOWN;
    req->http_version = PGREST_HTTP_VERSION_10;

    req->headers_in.content_length_n = -1;
    req->headers_in.keep_alive_n = -1;
    req->headers_out.content_length_n = -1;
    req->headers_out.last_modified_time = -1;

    req->http_state = PGREST_HTTP_READING_REQUEST_STATE;

    return req;
}

static void
pgrest_http_keepalive_handler(evutil_socket_t fd, short events, void *arg)
{
    ssize_t               n;
    pgrest_buffer_t      *buf;
    pgrest_connection_t  *conn;

    conn = (pgrest_connection_t *) arg;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http keepalive handler")));

    if (events & EV_TIMEOUT || conn->close) {
        pgrest_http_conn_close(conn);
        return;
    }

    buf = conn->buffer;
    Assert(buf->size == 0 && buf->pos == buf->start);

    n = pgrest_conn_recv(conn, buf->pos, buf->capacity);
    if (n == PGREST_AGAIN) {
        if (!pgrest_event_add(conn->rev, EV_READ, 0)) {
            pgrest_http_conn_close(conn);
        }

        return;
    }

    if (n == PGREST_ERROR) {
        pgrest_http_conn_close(conn);
        return;
    }

    if (n == 0) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client %s closed ke"
                             "epalive connection", conn->addr_text)));
        pgrest_http_conn_close(conn);
        return;
    }

    buf->size += n;

    conn->idle = 0;
    pgrest_conn_reusable(conn, 0);

    conn->data = pgrest_http_create_request(conn);
    if (conn->data == NULL) {
        pgrest_http_conn_close(conn);
        return;
    }

    conn->destroyed = 0;
    conn->sent = 0;

    conn->rev_handler = pgrest_http_header_handler;
    pgrest_http_header_handler(conn->fd, EV_READ, conn);
}

static inline size_t
pgrest_http_buffer_left(pgrest_http_request_t *req)
{
    if (req->header_only || (req->request_body && 
                                    req->request_body->split == 0)) {
        pgrest_buffer_consume(req->header_in, req->request_length);
        return req->header_in->size;
    }

    /* req->request_body->split == 1 */
    pgrest_buffer_reset(req->header_in);
    pgrest_buffer_consume(req->request_body->buffer, 
                          req->request_body->entity.len);

    return req->request_body->buffer->size;
}

static void
pgrest_http_set_keepalive(pgrest_http_request_t *req)
{
    size_t                     left;
    pgrest_connection_t       *conn;
    pgrest_http_connection_t  *hconn;
    pgrest_iovec_t             iovec;
    int                        tcp_nodelay;
    pgrest_conf_http_server_t *conf = req->http_conf;

    conn = req->conn;
    hconn = req->http_conn;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "set http keepalive handler")));

    hconn->prev_size = 0;
    if ((left = pgrest_http_buffer_left(req)) > 0) {
        /* the pipelined request */
        if (req->request_body && req->request_body->split) {
            iovec = pgrest_buffer_reserve(conn->pool, &conn->buffer, left);
            if (iovec.base == NULL) {
                pgrest_http_close_request(req, 0);
                return;
            }

            memcpy(iovec.base, req->request_body->buffer->pos, left);
            conn->buffer->size += left;
        }

        hconn->parse = 1;
    } else {
        Assert(conn->buffer->size == 0 && 
               conn->buffer->pos == conn->buffer->start);
        hconn->parse = 0;
    }

    /* guard against recursive call from pgrest_http_finalize_connection() */
    req->keepalive = 0;

    pgrest_http_free_request(req, 0);

    conn->data = hconn;
    conn->wev_handler = pgrest_http_empty_handler;

    if (left) {
        debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "pipelined request")));

        req = pgrest_http_create_request(conn);
        if (req == NULL) {
            pgrest_http_conn_close(conn);
            return;
        }

        req->pipeline = 1;

        conn->data = req;
        conn->sent = 0;
        conn->destroyed = 0;

        /* TODO event version dected */
        pgrest_event_del(conn->rev, EV_TIMEOUT);

        conn->rev_handler = pgrest_http_header_handler;
        pgrest_event_active(conn->rev, EV_READ);
        return;
    }

#ifdef HAVE_OPENSSL
    /* TODO ssl */
#endif

    conn->rev_handler = pgrest_http_keepalive_handler;

    if (!(pgrest_event_get_events(conn->wev) & EV_ET)) {
        if (!pgrest_event_del(conn->wev, EV_WRITE)) {
            pgrest_http_conn_close(conn);
            return;
        }
    }

    if (conn->tcp_nopush == PGREST_TCP_NOPUSH_SET) {
        if (pgrest_conn_tcp_push(conn) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "setsockopt("
                            "TCP_NOPUSH, 0) failed : %m")));

            pgrest_http_conn_close(conn);
            return;
        }

        conn->tcp_nopush = PGREST_TCP_NOPUSH_UNSET;
        tcp_nodelay = pgrest_tcp_nodelay_and_nopush ? 1 : 0;

    } else {
        tcp_nodelay = 1;
    }

    if (tcp_nodelay
        && conf->tcp_nodelay
        && conn->tcp_nodelay == PGREST_TCP_NODELAY_UNSET)
    {
        debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "tcp_nodelay")));

        if (setsockopt(conn->fd, IPPROTO_TCP, TCP_NODELAY,
                       (const void *) &tcp_nodelay, sizeof(int))
            == -1)
        {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "setsockopt("
                            "TCP_NODELAY, 1) failed : %m")));

            pgrest_http_conn_close(conn);
            return;
        }

        conn->tcp_nodelay = PGREST_TCP_NODELAY_SET;
    }

    conn->idle = 1;
    pgrest_conn_reusable(conn, 1);

    if (!pgrest_event_add(conn->rev, EV_READ, conf->keepalive_timeout)) {
        pgrest_http_conn_close(conn);
    }

    if (conn->ready) {
        pgrest_event_active(conn->rev, EV_READ);
    }
}

static void
pgrest_http_lingering_close_handler(evutil_socket_t fd, short events, void *arg)
{
    ssize_t                    n;
    pgrest_uint_t              timer;
    pgrest_connection_t       *conn;
    pgrest_http_request_t     *req;
    pgrest_conf_http_server_t *conf;
    struct timeval             now;
    unsigned char              buffer[PGREST_HTTP_LINGERING_BUFFER_SIZE];

    conn = (pgrest_connection_t *) arg;
    req = conn->data;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http lingering close handler")));

    if (events & EV_TIMEOUT) {
        pgrest_http_close_request(req, 0);
        return;
    }

    (void) pgrest_event_gettimeofday(pgrest_event_get_base(conn->rev), &now);

    timer = (pgrest_uint_t) req->lingering_time - (pgrest_uint_t) now.tv_sec;
    if ((pgrest_int_t) timer <= 0) {
        pgrest_http_close_request(req, 0);
        return;
    }

    do {
        n = pgrest_conn_recv(conn, buffer, PGREST_HTTP_LINGERING_BUFFER_SIZE);

        if (n == PGREST_ERROR || n == 0) {
            pgrest_http_close_request(req, 0);
            return;
        }

    } while (conn->ready);

    conf = req->http_conf;
    timer *= 1000;

    if (timer > conf->lingering_timeout) {
        timer = conf->lingering_timeout;
    }

    if (!pgrest_event_add(conn->rev, EV_READ, timer)) {
        pgrest_http_close_request(req, 0);
    }
}

static void
pgrest_http_set_lingering_close(pgrest_http_request_t *req)
{
    struct timeval             now;
    pgrest_connection_t       *conn;
    pgrest_conf_http_server_t *conf;
    short                      event;

    conn = req->conn;
    conf = req->http_conf;

    conn->rev_handler = pgrest_http_lingering_close_handler;

    (void) pgrest_event_gettimeofday(pgrest_event_get_base(conn->rev), &now);

    req->lingering_time = now.tv_sec + (time_t) (conf->lingering_time / 1000);
    if (!pgrest_event_add(conn->rev, EV_READ, conf->lingering_timeout)) {
        pgrest_http_close_request(req, 0);
    }

    conn->wev_handler = pgrest_http_empty_handler;

    event = pgrest_event_get_events(conn->wev);
    if (!(event & EV_ET)) {
        if (!pgrest_event_del(conn->wev, EV_WRITE)) {
            pgrest_http_close_request(req, 0);
        }
    }

    if (shutdown(conn->fd, SHUT_WR) == -1) {
        ereport(LOG,
                (errcode_for_socket_access(),
                 errmsg(PGREST_PACKAGE " " "shutdown(SHUT_WR) failed : %m")));

        pgrest_http_close_request(req, 0);
        return;
    }

    if (conn->ready) {
        pgrest_http_lingering_close_handler(conn->fd, EV_READ, conn);
    }
}

static void
pgrest_http_finalize_connection(pgrest_http_request_t *req)
{
    pgrest_conf_http_server_t *conf = req->http_conf;

    if (req->count != 1) {
        pgrest_http_close_request(req, 0);
        return;
    }

    if (!pgrest_worker_terminate
         && req->keepalive
         && conf->keepalive_timeout > 0)
    {
        pgrest_http_set_keepalive(req);
        return;
    }

    if (conf->lingering_close == PGREST_HTTP_LINGERING_ALWAYS
        || (conf->lingering_close == PGREST_HTTP_LINGERING_ON
            && (req->lingering_close
                || pgrest_http_buffer_left(req) > 0
                || req->conn->ready)))
    {
        pgrest_http_set_lingering_close(req);
        return;
    }

    pgrest_http_close_request(req, 0);
}

void
pgrest_http_close_request(pgrest_http_request_t *req, pgrest_int_t rc)
{
    pgrest_connection_t  *conn;

    conn = req->conn;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http requ"
                              "est count:%d", req->count)));

    if (req->count == 0) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "http request count is zero")));
    }

    req->count--;

    if (req->count) {
        return;
    }

    pgrest_http_free_request(req, rc);
    pgrest_http_conn_close(conn);
}

static void
pgrest_http_free_request(pgrest_http_request_t *req, pgrest_int_t rc)
{
    pgrest_mpool_t            *pool;
    struct linger              linger;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http close request")));

    if (req->pool == NULL) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "http request already closed")));
        return;
    }

    if (rc > 0 && (req->headers_out.status == 0 || req->conn->sent == 0)) {
        req->headers_out.status = rc;
    }

    if (req->conn->timedout) {
        if (req->http_conf->reset_timedout_connection) {
            linger.l_onoff = 1;
            linger.l_linger = 0;

            if (setsockopt(req->conn->fd, SOL_SOCKET, SO_LINGER,
                           (const void *) &linger, sizeof(struct linger)) == -1)
            {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsocko"
                                "pt(SO_LINGER) failed: %m")));
            }
        }
    }

    req->conn->destroyed = 1;

    /*
     * Setting r->pool to NULL will increase probability to catch double close
     * of request since the request object is allocated from its own pool.
     */
    pool = req->pool;
    req->pool = NULL;

    pgrest_mpool_destroy(pool);
}

#if 0
static void
pgrest_http_terminate_request(pgrest_http_request_t *req, pgrest_int_t rc)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http terminate"
                              " request count:%d", req->count)));

    pgrest_http_close_request(req, rc);
}

static void
pgrest_http_terminate_handler(pgrest_http_request_t *req)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http terminate"
                               "handler count:%d", req->count)));

    req->count = 1;

    pgrest_http_close_request(req, 0);
}
#endif

void
pgrest_http_finalize_request(pgrest_http_request_t *req, pgrest_int_t rc)
{

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http finalize request")));

    pgrest_http_finalize_connection(req);
}

int
pgrest_http_extend_header_buffer(pgrest_http_request_t *req)
{
    pgrest_conf_http_server_t *conf;
    size_t                     size;
    pgrest_iovec_t             iovec;
    pgrest_http_connection_t  *hconn = req->http_conn;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http extend header buffer")));

    conf = hconn->addr_conf->default_server;

    if (req->header_in->size > conf->client_max_header_size) {
        return PGREST_DECLINED;
    }

    if (req->header_in->size > req->header_in->capacity / 2) {
        size = ALLOCSET_BUFFER_MINSIZE;
    } else {
        size = req->header_in->capacity / 2 - req->header_in->size;
    }

    iovec = pgrest_buffer_reserve(req->conn->pool, &req->header_in, size);
    if (iovec.base == NULL) {
        return PGREST_ERROR;
    }

    req->conn->buffer = req->header_in;

    return PGREST_OK;
}

int
pgrest_http_extend_body_buffer(pgrest_http_request_t *req)
{
    pgrest_iovec_t             iovec;
    unsigned char             *old, *new;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http extend body buffer")));

    old = req->request_body->buffer->start;
    iovec = pgrest_buffer_reserve(req->pool, 
                                  &req->request_body->buffer,
                                  ALLOCSET_BUFFER_MINSIZE);
    if (iovec.base == NULL) {
        return PGREST_ERROR;
    }

    new = req->request_body->buffer->start;
    if (old != new && req->request_body->split == 0) {
        pgrest_http_header_align(&req->headers_in, old, new);

        req->path.base = new + (req->path.base - old);

        req->header_in = req->request_body->buffer;
        req->conn->buffer = req->request_body->buffer;
    }

    return PGREST_OK;
}

#if 0
static int
pgrest_http_find_virtual_server(pgrest_connection_t *conn,
                                pgrest_http_virtual_names_t *virtual_names, 
                                pgrest_string_t *host,
                                pgrest_http_request_t *req, 
                                pgrest_conf_http_server_t **confp)
{

    return PGREST_DECLINED;
}
#endif

static bool 
pgrest_http_validate_host(pgrest_string_t *host, pgrest_mpool_t *pool, pgrest_uint_t alloc)
{

    return true;
}

bool
pgrest_http_set_virtual_server(pgrest_http_request_t *req, 
                               pgrest_string_t       *host)
{
    return true;
}

bool
pgrest_http_process_host(pgrest_http_request_t *req, 
                         pgrest_param_t        *header,
                         size_t                 offset)
{
    int              rc;
    pgrest_string_t  host;

    if (req->headers_in.host == NULL) {
        req->headers_in.host = header;
    }

    host = header->value;

    rc = pgrest_http_validate_host(&host, req->pool, 0);

    if (rc == PGREST_DECLINED) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "client "
                             "send invalid host header")));
        pgrest_http_finalize_request(req, PGREST_HTTP_BAD_REQUEST);
        return false;
    }

    if (rc == PGREST_ERROR) {
        pgrest_http_close_request(req, PGREST_HTTP_INTERNAL_SERVER_ERROR);
        return false;
    }

    if (req->headers_in.server.len) {
        return true;
    }

    if (pgrest_http_set_virtual_server(req, &host) == false) {
        return false;
    }

    req->headers_in.server = host;

    return true;
}

bool
pgrest_http_test_expect(pgrest_http_request_t *req)
{
    int              n;
    pgrest_string_t *expect;

    if (req->expect_tested
        || req->headers_in.expect == NULL
        || req->http_version < PGREST_HTTP_VERSION_11)
    {
        return true;
    }

    req->expect_tested = 1;

    expect = &req->headers_in.expect->value;

    if (expect->len != sizeof("100-continue") - 1
        || strncasecmp((const char *)expect->base, "100-continue",
                           sizeof("100-continue") - 1)
           != 0)
    {
        return true;
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "send 100 Continue")));

    n = pgrest_conn_send(req->conn,
                         (unsigned char *) "HTTP/1.1 100 Continue" CRLF CRLF,
                         sizeof("HTTP/1.1 100 Continue" CRLF CRLF) - 1);

    if (n == sizeof("HTTP/1.1 100 Continue" CRLF CRLF) - 1) {
        return true;
    }

    /* we assume that such small packet should be send successfully */
    pgrest_http_close_request(req, PGREST_HTTP_INTERNAL_SERVER_ERROR);
    return false;
}

void
pgrest_http_block_reading(pgrest_http_request_t *req)
{
    short event;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "http reading blocked")));

    event = pgrest_event_get_events(req->conn->rev);
    if (!(event & EV_ET)) {
        if (!pgrest_event_del(req->conn->rev, EV_READ)) {
            pgrest_http_close_request(req, 0);
        }
    }
}
