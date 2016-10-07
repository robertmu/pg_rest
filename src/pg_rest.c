/* -------------------------------------------------------------------------
 *
 * pq_rest.c
 *   Entrypoint of pg_rest extension, and misc uncategolized functions
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"
#include "pg_rest.h"

PG_MODULE_MAGIC;

static MemoryContext pgrest_master_context;

static void
pgrest_os_init(void)
{
    pgrest_uint_t  i;

    pgrest_pagesize = getpagesize();
    for (i = pgrest_pagesize; i >>= 1; pgrest_pagesize_shift++) { /* void */ }
}

/*
 *
 * _PG_init
 *
 * Main entrypoint of pg_rest. It shall be invoked only once when postmaster
 * process is starting up
 *
 */
void
_PG_init(void)
{
    MemoryContext master_context;
    MemoryContext old_context;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "_PG_init")));

    /* hold listener,shm,ipc channel list */
    master_context = AllocSetContextCreate(TopMemoryContext,
                                           PGREST_PACKAGE " " "master",
                                           ALLOCSET_SMALL_SIZES);
    old_context = MemoryContextSwitchTo(master_context);

    /* os init */
    pgrest_os_init();

    /* initialize guc variables */
    pgrest_guc_init((char *) &pgrest_setting);

    /* initialize connection */
    pgrest_conn_init(pgrest_setting.worker_noconn);

    /* initialize http */
    pgrest_http_init(&pgrest_setting);

    /* initialize listener */
    pgrest_listener_init();

    /* initialize acceptor */
    pgrest_acceptor_init(pgrest_setting.worker_processes);

    /* initialize shared memory */
    pgrest_shm_init();

    /* initialize ipc */
    pgrest_ipc_init(pgrest_setting.worker_processes);

    /* initialize workers */
    pgrest_worker_init(pgrest_setting.worker_processes);

    pgrest_master_context = MemoryContextSwitchTo(old_context);
}

/*
 * Module unload callback()
 */
void
_PG_fini(void)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "_PG_fini")));

    pgrest_listener_fini();
    pgrest_shm_fini();
    pgrest_ipc_fini(pgrest_setting.worker_processes);

    MemoryContextDelete(pgrest_master_context);
}
