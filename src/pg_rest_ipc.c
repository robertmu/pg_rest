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
    evutil_socket_t      sockets[2];
    pgrest_connection_t *conn;
} pgrest_ipc_data_t;

static pgrest_ipc_data_t        *pgrest_ipc_data;
static pgrest_ipc_msg_handler_pt pgrest_ipc_msg_handlers[PGREST_IPC_CMD_MAX - 1];

bool 
pgrest_ipc_send_notify(int dest_worker, ev_uint8_t command)
{
    pgrest_connection_t *conn;

    conn = pgrest_ipc_data[dest_worker].conn;
    if (conn == NULL) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "could not send notify to "
                                 "worker %d : channel has been destroyed", 
                                  dest_worker)));
        return false;
    }

    if (pgrest_bufferevent_write(conn->bev, &command, 1) == -1) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "send notify to worker "
                                 "%d failed: out of memeory", dest_worker)));
        return false;
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d send %d command to worker %d",
                               pgrest_worker_index, (int) command, dest_worker)));
    return true;
}

static bool
pgrest_ipc_set_handler(int worker_index,
                       evutil_socket_t fd,
                       bufferevent_data_cb read_handler,
                       bufferevent_event_cb event_handler)
{
    pgrest_connection_t    *conn;

    conn = pgrest_conn_get(fd, true);
    if (conn == NULL) {
        return false;
    }

    conn->channel = 1;

    pgrest_bufferevent_setcb(conn->bev, read_handler, NULL, event_handler, conn);
    if (read_handler) {
        pgrest_bufferevent_enable(conn->bev, EV_READ);
    }

    pgrest_ipc_data[worker_index].conn = conn;

    return true;
}

static void
pgrest_ipc_channel_set_conn(pgrest_connection_t *conn)
{
    int i;

    for (i = 0; i < pgrest_setting.worker_processes; i++) {
        if (conn == pgrest_ipc_data[i].conn) {
            pgrest_ipc_data[i].conn = NULL;
            break;
        }
    }
}

static void 
pgrest_ipc_channel_read_handler(struct bufferevent *bev, void *arg)
{
    size_t               i;
    size_t               result;
    pgrest_connection_t *conn;
    ev_uint8_t           command[1024];

    conn = (pgrest_connection_t *) arg;

    result = pgrest_bufferevent_read(conn->bev, command, sizeof(command));
    for(i = 0; i < result; i++) {

        Assert(command[i] < PGREST_IPC_CMD_MAX);

        debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d received %d command",
                                   pgrest_worker_index, (int) command[i])));

        if (pgrest_ipc_msg_handlers[command[i]]) {
            pgrest_ipc_msg_handlers[command[i]]();
        }
    }
}

static void 
pgrest_ipc_channel_event_handler(struct bufferevent *bev, short events, void *arg)
{
    int                  err;
    pgrest_connection_t *conn;
    const char          *action;

    conn = (pgrest_connection_t *) arg;
    action = events & BEV_EVENT_READING ? "recv":"send";

    if (events & BEV_EVENT_ERROR) {
        err = EVUTIL_SOCKET_ERROR();
        if (PGREST_UTIL_ERR_RW_RETRIABLE(err)) {
            return;
        }

        ereport(COMMERROR,
                (errcode_for_socket_access(),
                 errmsg(PGREST_PACKAGE " " "could not %s notify: %m", action)));

        pgrest_conn_close(conn);
        pgrest_ipc_channel_set_conn(conn);
        return;
    }

    if (events & BEV_EVENT_EOF) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "recv() returned "
                                 "zero when receiving notify")));
        pgrest_conn_close(conn);
        pgrest_ipc_channel_set_conn(conn);
        return;
    }
}

void
pgrest_ipc_set_msg_handler(pgrest_ipc_cmd_e command, 
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
pgrest_ipc_worker_init(void *data)
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

        if (pgrest_ipc_set_handler(i,
                                   sockets[0], 
                                   NULL, 
                                   pgrest_ipc_channel_event_handler) == false) {
            return false;
        }

    }

    sockets = pgrest_ipc_data[pgrest_worker_index].sockets;
    evutil_closesocket(sockets[0]);

    if (pgrest_ipc_set_handler(pgrest_worker_index,
                               sockets[1], 
                               pgrest_ipc_channel_read_handler, 
                               pgrest_ipc_channel_event_handler) == false) {
        return false;
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d initialize ipc "
                              "channel successfull", pgrest_worker_index)));

    return true;
}

static bool
pgrest_ipc_worker_exit(void *data)
{
    int                  i;
    pgrest_connection_t *conn;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d ipc_channel_exit",
                               pgrest_worker_index)));

    for (i = 0; i < pgrest_setting.worker_processes; i++) {
        conn = pgrest_ipc_data[i].conn;
        if (conn) {
            pgrest_conn_close(conn);
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

        if (evutil_socketpair(LOCAL_SOCKETPAIR_AF, SOCK_STREAM, 0, sockets) == -1) {
            goto error;
		}

        if (evutil_make_socket_nonblocking(sockets[0]) == -1) {
            prompt = "nonblocking";
            goto fail;
        }

        if (evutil_make_socket_nonblocking(sockets[1]) == -1) {
            prompt = "nonblocking";
            goto fail;
        }

        if (evutil_make_socket_closeonexec(sockets[0]) == -1) {
            prompt = "closeonexec";
            goto fail;
        }

        if (evutil_make_socket_closeonexec(sockets[1]) == -1) {
            prompt = "closeonexec";
            goto fail;
        }

        pgrest_ipc_data[i].conn = NULL;
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "postmaster initialize ipc "
                              "channel successfull")));
    return true;

error:
    ereport(LOG,
            (errcode_for_socket_access(),
             errmsg(PGREST_PACKAGE " " "socketpair() failed : %m")));
    return false;

fail:
    ereport(LOG,
            (errcode_for_socket_access(),
             errmsg(PGREST_PACKAGE " " "make socket %s failed: %m", prompt)));
    pgrest_ipc_close_channel(sockets);
    return false;
}

static void
pgrest_ipc_fini1(int status, Datum arg)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "ipc fini1 %d", 
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
    on_proc_exit(pgrest_ipc_fini1, worker_processes);
}

void
pgrest_ipc_fini(int worker_processes)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "ipc fini")));
    pgrest_ipc_close(worker_processes);
}
