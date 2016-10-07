/* -------------------------------------------------------------------------
 *
 * pq_rest_acceptor.c
 *   accept/timer handler routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_acceptor.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct {
    slock_t     acceptor_mutex;
} pgrest_acceptor_data_t;

static pgrest_acceptor_data_t *pgrest_acceptor_data = NULL;
static struct event           *pgrest_acceptor_event_timer = NULL;

bool                           pgrest_acceptor_use_mutex = false;
bool                           pgrest_acceptor_mutex_held = false;
int                            pgrest_acceptor_disabled;

static bool 
pgrest_acceptor_shm_data_init(pgrest_shm_seg_t *seg)
{
    pgrest_acceptor_data = (pgrest_acceptor_data_t *) CACHELINEALIGN(seg->addr);
    SpinLockInit(&pgrest_acceptor_data->acceptor_mutex);

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "initialize acceptor_mutex successfull")));

    return true;
}

static void
pgrest_acceptor_shm_init(void)
{
    pgrest_shm_seg_t *seg;
    size_t            size;

    size = sizeof(pgrest_acceptor_data_t) + PG_CACHE_LINE_SIZE;
    seg = pgrest_shm_seg_add(PGREST_PACKAGE " " "acceptor data", 
                              size,
                              false, 
                              false);
    if (seg == NULL) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "initialize acceptor shared data failed")));
    }

    seg->init = pgrest_acceptor_shm_data_init;
}

static void
pgrest_acceptor_timer_handler(evutil_socket_t fd, short events, void *arg)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "timer_handler resume "
                           "accept in worker %d", pgrest_worker_index)));

    if(!pgrest_listener_resume()) {
        /* quit main event loop */
        pgrest_worker_event_error = true;
    }
}

static bool
pgrest_acceptor_worker_init(void *data)
{
    pgrest_acceptor_event_timer = pgrest_event_new(pgrest_worker_event_base, 
                                            -1, 
                                            0, 
                                            pgrest_acceptor_timer_handler, 
                                            NULL);
    if (!pgrest_acceptor_event_timer) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "worker %d create event "
                                 "object failed", pgrest_worker_index)));
        return false;
    }

    pgrest_event_priority_set(pgrest_acceptor_event_timer, 0);

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d initialize acceptor "
                               "successfull", pgrest_worker_index)));

    return true;
}

static bool
pgrest_acceptor_worker_exit(void *data)
{
    pgrest_event_free(pgrest_acceptor_event_timer);
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d acceptor_exit ",
                               pgrest_worker_index)));

    return true;
}

static void
pgrest_acceptor_close_conn(pgrest_connection_t *conn)
{
    evutil_socket_t  fd;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d acceptor close "
                              "connection", pgrest_worker_index)));

    pgrest_conn_free(conn);

    fd = conn->fd;
    conn->fd = (evutil_socket_t) -1;

    closesocket(fd);
}

void
pgrest_acceptor_accept_handler(evutil_socket_t afd, short events, void *arg)
{
    int                  err;
    evutil_socket_t      fd;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;
    pgrest_connection_t *lconn;
    SockAddr             sockaddr;
    int                  available;
#ifdef HAVE_ACCEPT4
    static int           use_accept4 = 1;
#endif

    lconn = (pgrest_connection_t *) arg;
    listener = lconn->listener;
    available = pgrest_setting.acceptor_multi_accept;

    ereport(DEBUG2, (errmsg(PGREST_PACKAGE " " "accept on %s multi_accept: %d",
                               listener->addr_text, 
                               pgrest_setting.acceptor_multi_accept)));

    do {
        sockaddr.salen = sizeof(sockaddr.addr);

#ifdef HAVE_ACCEPT4
        if (use_accept4) {
            fd = accept4(afd, (struct sockaddr *) &sockaddr.addr, &sockaddr.salen, 
                         SOCK_NONBLOCK);
        } else {
            fd = accept(afd, (struct sockaddr *) &sockaddr.addr, &sockaddr.salen);
        }
#else
        fd = accept(afd, (struct sockaddr *) &sockaddr.addr, &sockaddr.salen);
#endif

        if (fd == (evutil_socket_t) -1) {
            err = EVUTIL_SOCKET_ERROR();

            if (PGREST_UTIL_ERR_RW_RETRIABLE(err)) {
                debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "accept() not ready")));
                return;
            }

#ifdef HAVE_ACCEPT4
            ereport(COMMERROR,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "%s() failed: %m", 
                            use_accept4 ? "accept4" : "accept")));

            if (use_accept4 && err == ENOSYS) {
                use_accept4 = 0;
                continue;
            }
#else
            ereport(COMMERROR,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "accept() failed: %m")));
#endif

            if (err == ECONNABORTED) {
                if (available) {
                    continue;
                }
            }

            if (err == EMFILE || err == ENFILE) {
                if (! pgrest_listener_pause(true)) {
                    /* quit main event loop */
                    pgrest_worker_event_error = true;
                    return;
                }

                if (pgrest_acceptor_use_mutex ) {
                    pgrest_acceptor_disabled = 1;
                    pgrest_acceptor_mutex_held = false;
                    return;
                }

                if(!pgrest_event_add(pgrest_acceptor_event_timer, 
                                          EV_TIMEOUT, 
                                          pgrest_setting.acceptor_mutex_delay)) 
                {
                    /* quit main event loop */
                    pgrest_worker_event_error = true;
                }
            }

            return;
        }

        pgrest_acceptor_disabled = pgrest_setting.worker_noconn / 8
                                   - pgrest_conn_free_noconn;

        conn = pgrest_conn_get(fd, true);

        if (conn == NULL) {
            closesocket(fd);
            return;
        }

        conn->type = SOCK_STREAM;

        conn->sockaddr = palloc(sockaddr.salen);
        memcpy(conn->sockaddr, &sockaddr.addr, sockaddr.salen);

        if (evutil_make_socket_nonblocking(fd) == -1) {
            ereport(WARNING,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "make socket nonblocking failed: %m")));
            pgrest_acceptor_close_conn(conn);
            return;
        }

        conn->socklen = sockaddr.salen;
        conn->listener = listener;
        conn->local_sockaddr = listener->sockaddr;
        conn->local_socklen = listener->socklen;
#ifdef HAVE_UNIX_SOCKETS
        if (conn->sockaddr->sa_family == AF_UNIX) {
            conn->tcp_nopush = PGREST_TCP_NOPUSH_DISABLED;
            conn->tcp_nodelay = PGREST_TCP_NODELAY_DISABLED;
        }
#endif
        if (listener->addr_ntop) {
            conn->addr_text = palloc(listener->addr_text_max_len);
            (void) pgrest_util_inet_ntop(conn->sockaddr, 
                                         conn->socklen, 
                                         conn->addr_text, 
                                         listener->addr_text_max_len,
                                         false);
        }

        listener->handler(conn);

    } while (available);
}

bool
pgrest_acceptor_trylock_mutex(void)
{
    if (!SpinTryLockAcquire(&pgrest_acceptor_data->acceptor_mutex)) {

        debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d accept mutex locked",
                                   pgrest_worker_index)));

        if (pgrest_acceptor_mutex_held) {
            return true;
        }

        if (pgrest_listener_resume() == false) {
            SpinLockRelease(&pgrest_acceptor_data->acceptor_mutex);
            /* quit main event loop */
            pgrest_worker_event_error = true;
            return false;
        }

        pgrest_acceptor_mutex_held = true;
        return true;
    }

    debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d accept mutex locked failed: %d",
                               pgrest_worker_index, pgrest_acceptor_mutex_held)));

    if (pgrest_acceptor_mutex_held) {
        if (pgrest_listener_pause(false) == false) {
            /* quit main event loop */
            pgrest_worker_event_error = true;
            return false;
        }

        pgrest_acceptor_mutex_held = false;
    }

    return true;
}

void
pgrest_acceptor_unlock_mutex(void)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d accept unlock mutex",
                               pgrest_worker_index)));

    SpinLockRelease(&pgrest_acceptor_data->acceptor_mutex);
}

void
pgrest_acceptor_init(int worker_processes)
{
    if (worker_processes > 1 && pgrest_setting.acceptor_mutex) {
        pgrest_acceptor_use_mutex = true;
    } else {
        pgrest_acceptor_use_mutex = false;
    }

    pgrest_acceptor_shm_init();
    pgrest_worker_hook_add(pgrest_acceptor_worker_init, 
                           pgrest_acceptor_worker_exit, 
                           "acceptor", 
                           NULL, 
                           false);
}
