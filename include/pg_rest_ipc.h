/*-------------------------------------------------------------------------
 *
 * include/pg_rest_ipc.h
 *
 * ipc setup,handler routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_IPC_H
#define PG_REST_IPC_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef void (*pgrest_ipc_msg_handler_pt) (void);

typedef enum {
    PGREST_IPC_CMD_HTTP_PUSH_CHECK_ = 0,
    PGREST_IPC_CMD_HTTP_PUSH_DELETE,
    PGREST_IPC_CMD_DB_PUSH_CHECK,
    PGREST_IPC_CMD_MAX
} pgrest_ipc_cmd_e;

void pgrest_ipc_init(int worker_processes);
void pgrest_ipc_fini(int worker_processes);

void pgrest_ipc_close_channel(evutil_socket_t *sockets);
void pgrest_ipc_set_msg_handler(pgrest_ipc_cmd_e command, 
                           pgrest_ipc_msg_handler_pt handler);
bool pgrest_ipc_send_notify(int dest_worker, ev_uint8_t command);


#endif /* PG_REST_IPC_H */
