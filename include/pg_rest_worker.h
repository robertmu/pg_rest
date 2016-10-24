/*-------------------------------------------------------------------------
 *
 * include/pg_rest_worker.h
 *
 * worker process routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_WORKER_H
#define PG_REST_WORKER_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

#define PGREST_MAX_WORKERS            128
typedef bool (*pgrest_worker_hook_pt) (void *, void *);

extern int                 pgrest_worker_index;
extern bool                pgrest_worker_event_error;

void pgrest_worker_init(int worker_processes);
void pgrest_worker_hook_add(pgrest_worker_hook_pt  startup, 
                            pgrest_worker_hook_pt  shutdown,
                            const char            *name,
                            void                  *data,
                            bool                   head);
void pgrest_worker_info_print(void);

#endif /* PG_REST_WORKER_H */
