/* -------------------------------------------------------------------------
 *
 * pq_rest_ipc.c
 *   ipc routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_ipc.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

#ifdef WIN32
#define LOCAL_SOCKETPAIR_AF AF_INET
#else
#define LOCAL_SOCKETPAIR_AF AF_UNIX
#endif

typedef struct {
    pgrest_connection_t         *conn;
    pgrest_buffer_t             *buffer;
} pgrest_ipc_conn_t;

typedef struct {
    evutil_socket_t              sockets[2];
    pgrest_ipc_conn_t           *iconn;
} pgrest_ipc_data_t;

static pgrest_ipc_data_t        *pgrest_ipc_data;
static pgrest_ipc_msg_handler_pt pgrest_ipc_msg_handlers[PGREST_IPC_CMD_MAX];

bool 
pgrest_ipc_send_notify(int dest_worker, ev_uint8_t command)
{
    pgrest_ipc_conn_t *iconn;
    pgrest_iovec_t     iovec;

    iconn = pgrest_ipc_data[dest_worker].iconn;
    if (iconn == NULL) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "could not send notify "
                                 "to worker %d : channel has been destroyed", 
                                  dest_worker)));
        return false;
    }

    iovec = pgrest_buffer_reserve(iconn->conn->pool, &iconn->buffer, 1);
   *iovec.base = command;
    iconn->buffer->size++;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d send %d command"
                              " to worker %d", pgrest_worker_index, 
                                (int)command, dest_worker)));

    /* schedule the write */
    (void) pgrest_event_add(iconn->conn->rev, EV_WRITE, 0);

    return true;
}

static bool
pgrest_ipc_setup_handler(int worker_index,
                         struct event_base *base,
                         evutil_socket_t fd,
                         short events,
                         event_callback_fn handler)
{
    pgrest_connection_t *conn;
    pgrest_ipc_conn_t   *iconn;
    struct event        *ev;

    if ((conn = pgrest_conn_get(fd)) == NULL) {
        return false;
    }

    conn->channel = 1;

    if ((conn->pool = pgrest_mpool_create()) == NULL) {
        return false;
    }

    if((iconn = pgrest_mpool_alloc(conn->pool, 
                                   sizeof(pgrest_ipc_conn_t))) == NULL) 
    {
        return false;
    }

    iconn->conn = conn;
    if((iconn->buffer = pgrest_buffer_create(conn->pool, 1024)) == NULL) {
        return false;
    }

    ev = events & EV_READ ? conn->rev : conn->wev;
    if (pgrest_event_assign(ev, base, 
                            fd, events, handler, iconn) < 0) {
        return false;
    }

    pgrest_event_priority_set(ev, pgrest_acceptor_use_mutex ? 1 : 0);

    if (events & EV_READ) {
        (void) pgrest_event_add(ev, EV_READ, 0); 
    }

    pgrest_ipc_data[worker_index].iconn = iconn;

    return true;
}

static void
pgrest_ipc_set_conn(pgrest_ipc_conn_t *iconn)
{
    int i;

    for (i = 0; i < pgrest_setting.worker_processes; i++) {
        if (iconn == pgrest_ipc_data[i].iconn) {
            pgrest_ipc_data[i].iconn = NULL;
            break;
        }
    }
}

static void 
pgrest_ipc_read_handler(evutil_socket_t fd, short events, void *arg)
{
    size_t               i;
    ssize_t              result;
    pgrest_ipc_conn_t   *iconn;
    ev_uint8_t           command[128];

    iconn = (pgrest_ipc_conn_t *) arg;

    for (;;) {
        result = pgrest_conn_recv(iconn->conn, 
                                  (unsigned char *) command, 
                                  sizeof(command));

        if (result == 0) {
            ereport(WARNING, (errmsg(PGREST_PACKAGE " " "recv() returned"
                                      " zero when receiving notify")));
            goto fail;
        }

        if (result == PGREST_ERROR) {
            ereport(COMMERROR,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "recv() failed: %m")));
            goto fail;
        }

        if (result == PGREST_AGAIN) {
            return;
        }

        for(i = 0; i < result; i++) {

            Assert(command[i] < PGREST_IPC_CMD_MAX);

            debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d received"
                                      " %d command", pgrest_worker_index, 
                                       (int) command[i])));

            if (pgrest_ipc_msg_handlers[command[i]]) {
                pgrest_ipc_msg_handlers[command[i]]();
            }
        }
    }

    return;

fail:
    pgrest_conn_close(iconn->conn);
    pgrest_ipc_set_conn(iconn);
}

static void 
pgrest_ipc_write_handler(evutil_socket_t fd, short events, void *arg)
{
    ssize_t              result;
    pgrest_ipc_conn_t   *iconn;
    pgrest_buffer_t     *buf;

    iconn = (pgrest_ipc_conn_t *) arg;
    buf = iconn->buffer;

    if (buf->size == 0) {
        (void) pgrest_event_del(iconn->conn->wev, EV_WRITE);
        return;
    }

    result = pgrest_conn_send(iconn->conn, buf->pos, buf->size);

    if (result == 0) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "send() returned"
                                  " zero when send notify")));
        goto schedule;
    }

    if (result == PGREST_AGAIN) {
        goto schedule;
    }

    if (result == PGREST_ERROR) {
        ereport(COMMERROR,
                (errcode_for_socket_access(),
                 errmsg(PGREST_PACKAGE " " "send() failed: %m")));
        goto fail;
    }

    pgrest_buffer_consume(buf, result);

schedule:
    if (buf->size) {
        (void) pgrest_event_add(iconn->conn->rev, EV_WRITE, 0);
    }

    return;

fail:
    pgrest_conn_close(iconn->conn);
    pgrest_ipc_set_conn(iconn);
}

void
pgrest_ipc_setup_msg_handler(pgrest_ipc_cmd_e command, 
                           pgrest_ipc_msg_handler_pt handler)
{
    Assert(command < PGREST_IPC_CMD_MAX);
    pgrest_ipc_msg_handlers[command] = handler;
}

void
pgrest_ipc_close_channel(evutil_socket_t *sockets)
{
    evutil_closesocket(sockets[0]);
    evutil_closesocket(sockets[1]);
}

static bool
pgrest_ipc_worker_init(void *data, void *base)
{
    int                 i;
    evutil_socket_t    *sockets;
    int                 worker_processes = (intptr_t) data;

    for (i = 0; i < worker_processes; i++) {
        if (i == pgrest_worker_index) {
            continue;
        }

        sockets = pgrest_ipc_data[i].sockets;
        evutil_closesocket(sockets[1]);

        if (!pgrest_ipc_setup_handler(i,
                                    base,
                                    sockets[0], 
                                    EV_WRITE, 
                                    pgrest_ipc_write_handler)) 
        {
            return false;
        }
    }

    sockets = pgrest_ipc_data[pgrest_worker_index].sockets;
    evutil_closesocket(sockets[0]);

    if (!pgrest_ipc_setup_handler(pgrest_worker_index,
                                base,
                                sockets[1], 
                                EV_READ | EV_PERSIST, 
                                pgrest_ipc_read_handler)) 
    {
        return false;
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d initialize ipc"
                              " channel successfull", pgrest_worker_index)));

    return true;
}

static bool
pgrest_ipc_worker_exit(void *data, void *base)
{
    int                  i;
    pgrest_ipc_conn_t   *iconn;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d ipc_exit",
                               pgrest_worker_index)));

    for (i = 0; i < pgrest_setting.worker_processes; i++) {
        iconn = pgrest_ipc_data[i].iconn;
        if (iconn) {
            pgrest_conn_close(iconn->conn);
        }
    }

    return true;
}

static void
pgrest_ipc_close(int worker_processes)
{
    int              i;
    evutil_socket_t *sockets;

    for(i = 0; i < worker_processes; i++) {
        sockets = pgrest_ipc_data[i].sockets;
        pgrest_ipc_close_channel(sockets);
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "postmaster ipc close")));
}

static bool 
pgrest_ipc_channel_init(int worker_processes)
{
    int              i;
    evutil_socket_t *sockets;
    const char      *prompt;

    pgrest_ipc_data = palloc(sizeof(pgrest_ipc_data_t) * worker_processes);

    for(i = 0; i < worker_processes; i++) {
        sockets = pgrest_ipc_data[i].sockets;

        if (evutil_socketpair(LOCAL_SOCKETPAIR_AF, 
                              SOCK_STREAM, 0, sockets) == -1) {
            prompt = "socketpair()";
            goto fail;
        }

        if (evutil_make_socket_nonblocking(sockets[0]) == -1) {
            prompt = "nonblocking";
            goto error;
        }

        if (evutil_make_socket_nonblocking(sockets[1]) == -1) {
            prompt = "nonblocking";
            goto error;
        }

        if (evutil_make_socket_closeonexec(sockets[0]) == -1) {
            prompt = "closeonexec";
            goto error;
        }

        if (evutil_make_socket_closeonexec(sockets[1]) == -1) {
            prompt = "closeonexec";
            goto error;
        }

        pgrest_ipc_data[i].iconn = NULL;
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "postmaster initialize ipc "
                              "channel successfull")));
    return true;

error:
    pgrest_ipc_close_channel(sockets);
fail:
    ereport(LOG,
            (errcode_for_socket_access(),
             errmsg(PGREST_PACKAGE " " "%s failed: %m", prompt)));
    return false;
}

static void
pgrest_ipc_fini2(int status, Datum arg)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "ipc fini2 %d", 
                               DatumGetInt32(arg))));

    pgrest_ipc_fini(DatumGetInt32(arg));
}

void
pgrest_ipc_init(int worker_processes)
{
    if(! pgrest_ipc_channel_init(worker_processes)) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "initialize ipc channel failed")));
    }

    pgrest_worker_hook_add(pgrest_ipc_worker_init, 
                           pgrest_ipc_worker_exit,
                           "ipc channel",
                           (void *) (intptr_t) worker_processes,
                           false);

    on_proc_exit(pgrest_ipc_fini2, worker_processes);
}

void
pgrest_ipc_fini(int worker_processes)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "ipc fini")));
    pgrest_ipc_close(worker_processes);
}
