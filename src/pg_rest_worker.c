/* -------------------------------------------------------------------------
 *
 * pq_rest_worker.c
 *   worker routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_worker.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct {
#if PGSQL_VERSION >= 95
    pg_atomic_uint32    connections;
#else
    slock_t             stat_mutex;
    uint32              connections;
#endif
} pgrest_worker_stat_t;

typedef union
{
    pgrest_worker_stat_t stat;
    char                 pad[PG_CACHE_LINE_SIZE];
} pgrest_worker_stat_paded_t;

typedef struct {
    const char            *name;
    pgrest_worker_hook_pt  startup;
    pgrest_worker_hook_pt  shutdown;
    void                  *data;
} pgrest_worker_hook_t;

struct event_base                  *pgrest_worker_event_base;
static volatile sig_atomic_t        pgrest_worker_reconfigure = false;
static volatile sig_atomic_t        pgrest_worker_terminate = false;
static List                        *pgrest_worker_hooks = NIL;
static pgrest_worker_stat_paded_t  *pgrest_worker_stat = NULL;
static struct event                *pgrest_worker_event_timer = NULL;
static struct event                *pgrest_worker_event_sighup = NULL;
static struct event                *pgrest_worker_event_sigterm = NULL;

int                                 pgrest_worker_index;
bool                                pgrest_worker_event_error = false;
MemoryContext                       pgrest_worker_context;
#if PGREST_MM_REPLACE
MemoryContext                       pgrest_worker_event_context;
#endif

#if PGSQL_VERSION >= 95
static void pgrest_worker_main(Datum) pg_attribute_noreturn();
#else
static void pgrest_worker_main(Datum);
#endif

static void
pgrest_worker_signal_handler(evutil_socket_t fd, short events, void *arg)
{
    struct event *signal = arg;
    int           sigval = pgrest_event_get_signal(signal);

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d received signal %d ",
                               pgrest_worker_index, sigval)));

    switch (sigval) {
    case SIGHUP:
        pgrest_worker_reconfigure = true;
        break;
    case SIGTERM:
        pgrest_worker_terminate = true;
        break;
    default:
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "worker %d, signal %d ignored",
                                  pgrest_worker_index, sigval)));
    }
}

static void
pgrest_worker_signal_setup(struct event_base *event_base)
{
    pgrest_worker_event_sighup = pgrest_event_new(event_base,
                                           SIGHUP,
                                           EV_SIGNAL | EV_PERSIST,
                                           pgrest_worker_signal_handler,
                                           pgrest_event_self_cbarg());
    if (pgrest_worker_event_sighup == NULL) {
        goto fail;
    }

    pgrest_worker_event_sigterm = pgrest_event_new(event_base,
                                            SIGTERM,
                                            EV_SIGNAL | EV_PERSIST,
                                            pgrest_worker_signal_handler,
                                            pgrest_event_self_cbarg());
    if (pgrest_worker_event_sigterm == NULL) {
        goto fail;
    }

    pgrest_event_priority_set(pgrest_worker_event_sighup, 0);
    pgrest_event_priority_set(pgrest_worker_event_sigterm, 0);

    (void) pgrest_event_add(pgrest_worker_event_sighup, EV_SIGNAL, 0);
    (void) pgrest_event_add(pgrest_worker_event_sigterm, EV_SIGNAL, 0);

    BackgroundWorkerUnblockSignals();

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d initialize signal "
                              "successfull", pgrest_worker_index)));
    return;

fail:
    ereport(ERROR, (errmsg(PGREST_PACKAGE " " "create event object failed")));
}

static void
pgrest_worker_guc_check(void)
{
#if defined HAVE_GETRLIMIT
    struct rlimit     rlim;
    int               limit;

    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " 
                                 "getrlimit(RLIMIT_NOFILE) failed: %m, ignored")));
    } else {
        if (pgrest_setting.worker_noconn > (int) rlim.rlim_cur
            && (pgrest_setting.worker_nofile == -1
                || pgrest_setting.worker_noconn > pgrest_setting.worker_nofile)) {
            limit = (pgrest_setting.worker_nofile == -1) ?
                         (int) rlim.rlim_cur : pgrest_setting.worker_nofile;
            ereport(WARNING,
                    (errmsg(PGREST_PACKAGE " " "%d worker_connections "
                            "open file resource limit %d",
                             pgrest_setting.worker_noconn, limit)));
        }
    }
#endif
}

static void
pgrest_worker_register(int worker_processes)
{
    BackgroundWorker  worker;
    int               i;

    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    worker.bgw_main = pgrest_worker_main;
#if PGSQL_VERSION >= 94
    worker.bgw_notify_pid = 0;
#endif

    for (i = 0; i < worker_processes; i++) {
        snprintf(worker.bgw_name, BGW_MAXLEN, PGREST_PACKAGE " " "worker");
        worker.bgw_main_arg = Int32GetDatum(i);

        RegisterBackgroundWorker(&worker);
    }
}

static bool 
pgrest_worker_stat_data_init(pgrest_shm_seg_t *seg)
{
    int i;

    pgrest_worker_stat = (pgrest_worker_stat_paded_t *) CACHELINEALIGN(seg->addr);

    for (i = 0; i < pgrest_setting.worker_processes; i++) {
#if PGSQL_VERSION >= 95
        pg_atomic_init_u32(&pgrest_worker_stat[i].stat.connections, 0);
#else
        SpinLockInit(&pgrest_worker_stat[i].stat.stat_mutex);
        pgrest_worker_stat[i].stat.connections = 0;
#endif
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d initialize shared "
                              "memory successfull", pgrest_worker_index)));
    return true;
}

static void
pgrest_worker_stat_init(int worker_processes)
{
    pgrest_shm_seg_t *seg;
    size_t            size;

    size = sizeof(pgrest_worker_stat_paded_t) * worker_processes + PG_CACHE_LINE_SIZE;
    seg = pgrest_shm_seg_add(PGREST_PACKAGE " " "worker stat", 
                              size,
                              false, 
                              false);
    if (seg == NULL) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "initialize worker stat failed")));
    }

    seg->init = pgrest_worker_stat_data_init;
}

void
pgrest_worker_hook_add(pgrest_worker_hook_pt  startup, 
                       pgrest_worker_hook_pt  shutdown,
                       const char            *name,
                       void                  *data,
                       bool                   head)
{
    pgrest_worker_hook_t  *hook;

    hook = (pgrest_worker_hook_t *) palloc0fast(sizeof(pgrest_worker_hook_t));

    hook->name = name;
    hook->startup = startup;
    hook->shutdown = shutdown;
    hook->data = data;

    if (head) {
        pgrest_worker_hooks = lcons(hook, pgrest_worker_hooks);
    } else {
        pgrest_worker_hooks = lappend(pgrest_worker_hooks, hook);
    }
}

static void
pgrest_worker_process_init(struct event_base *event_base)
{
    ListCell         *cell;
#if defined HAVE_GETRLIMIT
    struct rlimit     rlim;

    if (pgrest_setting.worker_nofile != -1) {
        rlim.rlim_cur = (rlim_t) pgrest_setting.worker_nofile;
        rlim.rlim_max = (rlim_t) pgrest_setting.worker_nofile;

        if (setrlimit(RLIMIT_NOFILE, &rlim) == -1) {
            ereport(WARNING, (errmsg(PGREST_PACKAGE " " "setrlimit(RLIMIT_NOFILE, %d) "
                                     "failed: %m", pgrest_setting.worker_nofile)));
        }
    }
#endif

#ifndef WIN32
    if (pgrest_setting.worker_priority != 0) {
        if (setpriority(PRIO_PROCESS, 0, pgrest_setting.worker_priority) == -1) {
            ereport(WARNING, (errmsg(PGREST_PACKAGE " " "setpriority(%d) failed: %m",
                                      pgrest_setting.worker_priority)));
        }
    }
#endif

    foreach(cell, pgrest_worker_hooks) {
        pgrest_worker_hook_t *hook = lfirst(cell);

        debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d invoke %s module "
                                  "startup hook", pgrest_worker_index, hook->name)));

        if (hook->startup) {
            if(hook->startup(hook->data) == false) {
                ereport(ERROR,
                        (errmsg(PGREST_PACKAGE " " "initialize subsystems failed")));
            }
        }
    }

    pgrest_worker_signal_setup(event_base);

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d initialize subsystems "
                              "successfull", pgrest_worker_index)));

}

static void
pgrest_worker_process_exit(struct event_base *event_base)
{
    ListCell         *cell;

    foreach(cell, pgrest_worker_hooks) {
        pgrest_worker_hook_t *hook = lfirst(cell);
        if (hook->shutdown) {
            (void)hook->shutdown(hook->data);
        }
    }

    ereport(LOG,(errmsg(PGREST_PACKAGE " " "worker %d exit", 
                         pgrest_worker_index)));
}

static void
pgrest_worker_timer_handler(evutil_socket_t fd, short events, void *arg)
{
    debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d notify_handler "
                              "timeout", pgrest_worker_index)));
}

static void
pgrest_worker_event_dispatch(struct event_base *event_base)
{
    int save_err;
    int err = 0;

    if (pgrest_acceptor_use_mutex == false) {
        debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d event dispatch "
                               "(accept_mutex:off)", pgrest_worker_index)));
        goto dispatch;
    }

    if (pgrest_acceptor_disabled > 0) {
        debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d event dispatch "
                                "(accept_mutex:on busy:yes accept:off)",
                                 pgrest_worker_index)));
        pgrest_acceptor_disabled--;
        goto dispatch;
    }

    if (pgrest_acceptor_trylock_mutex() == false) {
        goto done;
    }

    if (pgrest_acceptor_mutex_held) {
        debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d event dispatch "
                                "(accept_mutex:on busy:no accept:on)",
                                 pgrest_worker_index)));
    } else {
        debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d event dispatch "
                                "(accept_mutex:on busy:no accept:off)",
                                 pgrest_worker_index)));

        if(!pgrest_event_add(pgrest_worker_event_timer, 
                                  EV_TIMEOUT,
                                  pgrest_setting.acceptor_mutex_delay)) 
        {
            /* quit main event loop */
            pgrest_worker_event_error = true;
            goto done;
        }
    }

    /* timeout or accept handler(low priority) */
    if (pgrest_event_base_loop(event_base, EVLOOP_ONCE) == -1) {
        err = 1;
        save_err = EVUTIL_SOCKET_ERROR();
    }

    /* already invoke timeout or accept handler */
    debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d event dispatch low "
                              "priority handler invoke", pgrest_worker_index)));

    /* release accept lock */
    if (pgrest_acceptor_mutex_held) {
        pgrest_acceptor_unlock_mutex();
    } else {
        if (!pgrest_event_del(pgrest_worker_event_timer, EV_TIMEOUT)) {
            /* quit main event loop */
            pgrest_worker_event_error = true;
        }
    }

    if(err) {
        EVUTIL_SET_SOCKET_ERROR(save_err);
        goto fail;
    }

    /* Now we invoke I/O handler(high priority) */
    if (pgrest_event_base_loop(event_base, EVLOOP_NONBLOCK) == -1) {
        goto fail;
    }

    debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d event dispatch high "
                              "priority handler invoke", pgrest_worker_index)));
    return;

dispatch:
    if (pgrest_event_base_loop(event_base, EVLOOP_ONCE) == -1) {
        goto fail;
    }
    return;

fail:
    ereport(WARNING,
            (errmsg(PGREST_PACKAGE " " "worker %d event_base_loop() failed: %m",
                     pgrest_worker_index)));

done:
    return;
}

static void
pgrest_worker_mm_init(void)
{
    pgrest_worker_context = AllocSetContextCreate(TopMemoryContext,
                                                  PGREST_PACKAGE " " "worker",
                                                  ALLOCSET_DEFAULT_SIZES);
	MemoryContextSwitchTo(pgrest_worker_context);

#if PGREST_MM_REPLACE
    pgrest_worker_event_context = AllocSetContextCreate(TopMemoryContext,
                                                  PGREST_PACKAGE " " "worker event",
                                                  ALLOCSET_DEFAULT_SIZES);
#endif

}

static void
pgrest_worker_mm_fini(void)
{
    /*
     * We don't delete currentMemoryContext(must be pgrest_worker_context)
     */
#if PGREST_MM_REPLACE
    MemoryContextDelete(pgrest_worker_event_context);
#endif
}

#if PGREST_DEBUG
static void
pgrest_worker_info_debug(void)
{
    pgrest_listener_info_print();
    pgrest_shm_info_print();
    pgrest_worker_info_print();
}
#endif

static struct event_base *
pgrest_worker_event_init(void)
{
    struct event_base *event_base;
    int    nqueue;

    /* event memory hook setup*/
#if PGREST_MM_REPLACE
    event_set_mem_functions(pgrest_util_palloc, 
                            pgrest_util_repalloc, 
                            pgrest_util_pfree);
#endif

    /* event log hook setup */
    event_set_log_callback(pgrest_util_event_log);

    /* event abort hook setup */
    event_set_fatal_callback(pgrest_util_event_fatal);

    /* create event base */
    event_base = pgrest_event_base_new();
    if (event_base == NULL) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "create event base object failed")));
    }

    /* initialize priority queue setup */
    nqueue = pgrest_acceptor_use_mutex ? 2 : 1;
    if (pgrest_event_base_priority_init(event_base, nqueue) == -1) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "initialize priority queue failed")));
    }

    /* initialize event notify */
    if (pgrest_acceptor_use_mutex) {
        pgrest_worker_event_timer = pgrest_event_new(event_base, 
                                              -1, 
                                              0, 
                                              pgrest_worker_timer_handler, 
                                              NULL);
        if (pgrest_worker_event_timer == NULL) {
            ereport(ERROR, (errmsg(PGREST_PACKAGE " " "create event object failed")));
        }

        pgrest_event_priority_set(pgrest_worker_event_timer, 0);
    }

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d initialize event "
                              "successfull", pgrest_worker_index)));

    return event_base;
}

static void
pgrest_worker_event_fini(struct event_base *event_base)
{
    pgrest_event_free(pgrest_worker_event_timer);
    pgrest_event_free(pgrest_worker_event_sighup);
    pgrest_event_free(pgrest_worker_event_sigterm);

    pgrest_event_base_free(event_base);
    pgrest_event_global_shutdown();
}

static void
pgrest_worker_main(Datum arg)
{
    pgrest_worker_index = DatumGetInt32(arg);

    pgrest_worker_mm_init();
    pgrest_worker_event_base = pgrest_worker_event_init();

#if PGREST_DEBUG
    pgrest_worker_info_debug();
#endif

    /* initialize subsystems */
    pgrest_worker_process_init(pgrest_worker_event_base);

    ereport(LOG,(errmsg(PGREST_PACKAGE " " "worker %d started",
                         pgrest_worker_index)));

    PG_TRY();
    {
        /*
	     * Main loop: do this until the SIGTERM handler tells us to terminate
         */
        for (;;) {
            debug_log(DEBUG2, (errmsg(PGREST_PACKAGE " " "worker %d event cycle", 
                                       pgrest_worker_index)));

            pgrest_worker_event_dispatch(pgrest_worker_event_base);

            /* got SIGTERM */
            if (pgrest_worker_terminate) {
                ereport(LOG,(errmsg(PGREST_PACKAGE " " "worker %d got SIGTERM",
                                     pgrest_worker_index)));
                break;
            }

            /* got SIGHUP */
            if (pgrest_worker_reconfigure) {
                ereport(LOG,(errmsg(PGREST_PACKAGE " " "worker %d got SIGHUP",
                                     pgrest_worker_index)));
                pgrest_worker_reconfigure = false;
                ProcessConfigFile(PGC_SIGHUP);
                /* TODO reapply setting */
            }

            /* event internal error */
            if (pgrest_worker_event_error) {
                ereport(LOG,(errmsg(PGREST_PACKAGE " " "worker %d got event "
                                    "internal error", pgrest_worker_index)));
                break;
            }
        }
    }
    PG_CATCH();
    {
        if (pgrest_acceptor_mutex_held) {
            pgrest_acceptor_unlock_mutex();
        }
        PG_RE_THROW();
    }
    PG_END_TRY();

    pgrest_worker_process_exit(pgrest_worker_event_base);
    pgrest_worker_event_fini(pgrest_worker_event_base);
    pgrest_worker_mm_fini();

    proc_exit(0);
}

static StringInfo
pgrest_worker_info_format(List *subsystems)
{
    StringInfo             result;
    ListCell              *cell;
    pgrest_worker_hook_t  *hook;

    result = makeStringInfo();

    foreach(cell, subsystems) {
        hook = lfirst(cell);

        appendStringInfo(result,"\n");

        appendStringInfo(result,
            "!\tmodule name %s\n", hook->name);
        appendStringInfo(result,
            "!\tmodule startup %p\n", hook->startup);
        appendStringInfo(result,
            "!\tmodule shutdown %p\n", hook->shutdown);
    }

    return result;
}

void
pgrest_worker_info_print(void)
{
    StringInfo buffer;

    buffer = pgrest_worker_info_format(pgrest_worker_hooks);

    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print %s subsystems ++++++++++", 
                            !IsUnderPostmaster ? "postmaster" : "worker")));
    ereport(DEBUG1,(errmsg("\n%s", buffer->data)));
    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print %s subsystems ----------",
                            !IsUnderPostmaster ? "postmaster": "worker")));

    pfree(buffer->data);
    pfree(buffer);
}

void
pgrest_worker_init(int worker_processes)
{
    pgrest_worker_guc_check();
    pgrest_worker_stat_init(worker_processes);
    pgrest_worker_register(worker_processes);
#if PGREST_DEBUG
    pgrest_worker_info_print();
#endif 
}
