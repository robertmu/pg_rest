/*-------------------------------------------------------------------------
 *
 * include/pg_rest_acceptor.h
 *
 * accept/timer handler routines
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_ACCEPTOR_H
#define PG_REST_ACCEPTOR_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

extern bool  pgrest_acceptor_use_mutex;
extern bool  pgrest_acceptor_mutex_held;
extern int   pgrest_acceptor_disabled;

void pgrest_acceptor_accept_handler(evutil_socket_t fd, 
                                    short event, 
                                    void *arg);
void pgrest_acceptor_init(int worker_processes);
bool pgrest_acceptor_trylock_mutex(void);
void pgrest_acceptor_unlock_mutex(void);


#endif /* PG_REST_ACCEPTOR_H */
