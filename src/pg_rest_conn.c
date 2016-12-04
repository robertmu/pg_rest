/* -------------------------------------------------------------------------
 *
 * pq_rest_conn.c
 *   connection routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_conn.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

static pgrest_connection_t *pgrest_conn_conns;
static pgrest_connection_t *pgrest_conn_free_conns;
static dlist_head           pgrest_conn_reuse_conns = 
                                DLIST_STATIC_INIT(pgrest_conn_reuse_conns);

int                         pgrest_conn_free_noconn;

static void pgrest_conn_drain(void);

pgrest_connection_t *
pgrest_conn_get(evutil_socket_t fd)
{
    pgrest_connection_t  *conn;

    conn = pgrest_conn_free_conns;

    if (conn == NULL) {
        pgrest_conn_drain();
        conn = pgrest_conn_free_conns;
    }

    if (conn == NULL) {
        ereport(WARNING,
                (errmsg(PGREST_PACKAGE " " "%d worker_connections are not "
                        "enough in worker %d",
                         pgrest_setting.worker_noconn, pgrest_worker_index)));
        return NULL;
    }

    pgrest_conn_free_conns = conn->data;
    pgrest_conn_free_noconn--;

    MemSet(conn, 0, sizeof(pgrest_connection_t));

    conn->fd = fd;
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d get connection %d "
                              "successful", pgrest_worker_index, conn->fd)));
    return conn;
}

static void
pgrest_conn_drain(void)
{
    int                   i;
    dlist_node           *node;
    pgrest_connection_t  *conn;

    for (i = 0; i < 32; i++) {
        if (dlist_is_empty(&pgrest_conn_reuse_conns)) {
            break;
        }

        node = dlist_tail_node(&pgrest_conn_reuse_conns);
        conn = dlist_container(pgrest_connection_t, elem, node);

        ereport(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d reusing "
                                "connection", pgrest_worker_index)));
        conn->close = 1;
        /* XXX call event handler to close connection */
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d drain "
                              "connection", pgrest_worker_index)));
}

void
pgrest_conn_reusable(pgrest_connection_t *conn, int reusable)
{
    debug_log(DEBUG1,
              (errmsg(PGREST_PACKAGE " " "worker %d reusable connection: "
                      "%d", pgrest_worker_index, reusable)));

    if (conn->reusable) {
        dlist_delete(&conn->elem);
    }

    conn->reusable = reusable;

    if (reusable) {
        dlist_push_head(&pgrest_conn_reuse_conns, &conn->elem);
    }
}

void
pgrest_conn_free(pgrest_connection_t *conn)
{
    debug_log(DEBUG1,
            (errmsg(PGREST_PACKAGE " " "worker %d free connection",
                     pgrest_worker_index)));

    conn->data = pgrest_conn_free_conns;
    pgrest_conn_free_conns = conn;
    pgrest_conn_free_noconn++;
}

void
pgrest_conn_close(pgrest_connection_t *conn)
{
    evutil_socket_t  fd;

    if (conn->fd == (evutil_socket_t) -1) {
        ereport(WARNING,
                (errmsg(PGREST_PACKAGE " " "worker %d connection %d already"
                         " closed", pgrest_worker_index, conn->fd)));
        return;
    }

    (void) pgrest_event_del(conn->rev, EV_READ);
    (void) pgrest_event_del(conn->wev, EV_WRITE);

    pgrest_conn_reusable(conn, 0);
    pgrest_conn_free(conn);

    fd = conn->fd;
    conn->fd = (evutil_socket_t) -1;

    closesocket(fd);
}

ssize_t
pgrest_conn_recv(pgrest_connection_t *conn, unsigned char *buf, size_t size)
{
    ssize_t n;
    int     err;

    do {
        n = recv(conn->fd, buf, size, 0);

        if (n == 0) {
            conn->ready = 0;
            return 0;
        }

        if (n > 0) {
            if ((size_t) n < size
                && !(pgrest_event_get_events(conn->rev) & EV_ET))
            {
                conn->ready = 0;
            }

            return n;
        }

        err = EVUTIL_SOCKET_ERROR();
        if (PGREST_UTIL_ERR_RW_RETRIABLE(err)) {
            n = PGREST_AGAIN;
        } else {
            n = PGREST_ERROR;
            break;
        }
    } while (err == EINTR);

    conn->ready = 0;

    return n;
}

ssize_t
pgrest_conn_send(pgrest_connection_t *conn, unsigned char *buf, size_t size)
{
    ssize_t n;
    int     err;

    for ( ;; ) {
        n = send(conn->fd, buf, size, 0);

        if (n >= 0) {
            conn->sent += n;
            return n;
        }

        err = EVUTIL_SOCKET_ERROR();
        if (PGREST_UTIL_ERR_RW_RETRIABLE(err)) {
            if (err == EAGAIN) {
                return PGREST_AGAIN;
            }
        } else {
            return PGREST_ERROR;
        }
    }
}

bool
pgrest_conn_local_sockaddr(pgrest_connection_t *conn, 
                           pgrest_string_t *s,
                           bool port)
{
    socklen_t             len;
    pgrest_uint_t         addr;
    pgrest_sockaddr_t     sa;
    struct sockaddr_in   *sin;
#ifdef HAVE_IPV6
    pgrest_uint_t         i;
    struct sockaddr_in6  *sin6;
#endif

    addr = 0;
    if (conn->local_socklen) {
        switch (conn->local_sockaddr->sa_family) {
#ifdef HAVE_IPV6
        case AF_INET6:
            sin6 = (struct sockaddr_in6 *) conn->local_sockaddr;

            for (i = 0; addr == 0 && i < 16; i++) {
                addr |= sin6->sin6_addr.s6_addr[i];
            }
            break;
#endif
#ifdef HAVE_UNIX_SOCKETS
        case AF_UNIX:
            addr = 1;
            break;
#endif
        default: /* AF_INET */
            sin = (struct sockaddr_in *) conn->local_sockaddr;
            addr = sin->sin_addr.s_addr;
            break;
        }
    }

    if (addr == 0) {
        len = sizeof(pgrest_sockaddr_t);

        if (getsockname(conn->fd, &sa.sockaddr, &len) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "getsockname() failed: %m")));
            return false;
        }

        conn->local_sockaddr = pgrest_mpool_alloc(conn->pool, len);
        if (conn->local_sockaddr == NULL) {
            return false;
        }

        memcpy(conn->local_sockaddr, &sa, len);
        conn->local_socklen = len;
    }

    if (s == NULL) {
        return true;
    }

    (void) pgrest_inet_ntop(conn->local_sockaddr, 
                            conn->local_socklen, 
                            (char *)s->base, 
                            s->len,
                            port);
    s->len = strlen((char *)s->base);
    return true;
}

#if defined(__FreeBSD__)
int
pgrest_conn_tcp_nopush(pgrest_connection_t *conn)
{
    int  tcp_nopush;

    tcp_nopush = 1;

    return setsockopt(conn->fd, IPPROTO_TCP, TCP_NOPUSH,
                      (const void *) &tcp_nopush, sizeof(int));
}

int
pgrest_conn_tcp_push(pgrest_connection_t *conn)
{
    int  tcp_nopush;

    tcp_nopush = 0;

    return setsockopt(conn->fd, IPPROTO_TCP, TCP_NOPUSH,
                      (const void *) &tcp_nopush, sizeof(int));
}
#elif defined(__linux__)
int
pgrest_conn_tcp_nopush(pgrest_connection_t *conn)
{
    int  cork;

    cork = 1;

    return setsockopt(conn->fd, IPPROTO_TCP, TCP_CORK,
                      (const void *) &cork, sizeof(int));
}

int
pgrest_conn_tcp_push(pgrest_connection_t *conn)
{
    int  cork;

    cork = 0;

    return setsockopt(conn->fd, IPPROTO_TCP, TCP_CORK,
                      (const void *) &cork, sizeof(int));
}
#else
int
pgrest_conn_tcp_nopush(pgrest_connection_t *conn)
{
    return 0;
}

int
pgrest_conn_tcp_push(pgrest_connection_t *conn)
{
    return 0;
}
#endif

static bool
pgrest_conn_worker_init(void *data, void *base)
{
    int                  i;
    pgrest_connection_t *conn;
    pgrest_connection_t *next;
    int                  worker_noconn = (intptr_t) data;

    pgrest_conn_conns = palloc0(sizeof(pgrest_connection_t) * worker_noconn);

    conn = pgrest_conn_conns;
    i = worker_noconn;
    next = NULL;

    do {
        i--;

        conn[i].data = next;
        conn[i].fd = (evutil_socket_t) -1;
        conn[i].rev = pgrest_event_new(NULL, -1, 0, NULL, NULL);
        conn[i].wev = pgrest_event_new(NULL, -1, 0, NULL, NULL);

        next = &conn[i];
    } while (i);

    pgrest_conn_free_conns = next;
    pgrest_conn_free_noconn = worker_noconn;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d initialize "
                              "connection pool successfull",
                               pgrest_worker_index)));

    return true;
}

static bool
pgrest_conn_worker_exit(void *data, void *base)
{
    int                   i;
    pgrest_connection_t  *conn;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d conn_exit",
                               pgrest_worker_index)));

    conn = pgrest_conn_conns;
    
    for (i = 0; i < pgrest_setting.worker_noconn; i++) {
        if (conn[i].fd != -1
            && !conn[i].channel) {
            ereport(WARNING,
                    (errmsg(PGREST_PACKAGE " " "worker %d open socket %d "
                            "left in connection %d", pgrest_worker_index,
                             conn[i].fd, i)));
        }

        /* free event */
        (void) pgrest_event_del(conn[i].rev, EV_READ);
        (void) pgrest_event_del(conn[i].wev, EV_WRITE);
        pgrest_event_free(conn[i].rev);
        pgrest_event_free(conn[i].wev);
    }

    return true;
}

void
pgrest_conn_init(int worker_noconn)
{
    pgrest_worker_hook_add(pgrest_conn_worker_init, 
                           pgrest_conn_worker_exit,
                           "connection",
                           (void *) (intptr_t) worker_noconn,
                           false);
}
