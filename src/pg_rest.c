/*
 * pg_rest.c
 *
 * Entrypoint of pg_rest extension, and misc uncategolized functions
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 * IDENTIFICATION
 *		src/pg_rest.c
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"

#include "fmgr.h"
#include "utils/builtins.h"
#include "tcop/utility.h"

#include "pg_rest.h"

PG_MODULE_MAGIC;

void pgrest_worker_main(Datum) pg_attribute_noreturn();

/* Custom GUC variables */
char *pgrest_listen_addresses;
bool  pgrest_http_enabled;
int   pgrest_http_port;
char *pgrest_crud_url_prefix;
char *pgrest_doc_url_prefix;
char *pgrest_sql_url_prefix;
bool  pgrest_https_enabled;
int   pgrest_https_port;
char *pgrest_https_ssl_key;
int   pgrest_max_workers;

/* Flags set by signal handlers */
volatile sig_atomic_t pgrest_reconfigure = false;
volatile sig_atomic_t pgrest_terminate = false;
volatile bool pgrest_postmaster_death = false;

static bool
pgrest_extra_check_hook(char **newvalue, void **extra, GucSource source)
{

    return true;
}

static void
pgrest_init_misc_guc(void)
{
    DefineCustomStringVariable("pg_rest.listen_addresses",
                               gettext_noop("Sets the host name or IP address(es) to listen to."),
                               NULL,
                               &pgrest_listen_addresses,
                               "localhost",
                               PGC_POSTMASTER,
                               GUC_NOT_IN_SAMPLE|GUC_LIST_INPUT,
                               pgrest_extra_check_hook,
                               NULL, NULL);

    DefineCustomBoolVariable("pg_rest.http_enabled",
                             gettext_noop("Whether to enable listening for client requests on an HTTP port."),
                             NULL,
                             &pgrest_http_enabled,
                             true,
                             PGC_POSTMASTER,
                             GUC_NOT_IN_SAMPLE,
                             NULL, NULL, NULL);

    DefineCustomIntVariable("pg_rest.http_port",
                            gettext_noop("Network port to listen on for HTTP requests."),
                            NULL,
                            &pgrest_http_port,
                            8080,
                            1,
                            65535,
                            PGC_POSTMASTER,
                            GUC_NOT_IN_SAMPLE,
                            NULL, NULL, NULL);

    DefineCustomStringVariable("pg_rest.crud_url_prefix",
                               gettext_noop("URL prefix of the CRUD (Create-Read-Update-Delete) endpoint."),
                               NULL,
                               &pgrest_crud_url_prefix,
                               "/crud/",
                               PGC_SUSET,
                               GUC_NOT_IN_SAMPLE,
                               NULL, NULL, NULL);

    DefineCustomStringVariable("pg_rest.doc_url_prefix",
                               gettext_noop("URL prefix of the JSON Document (DOC) endpoint."),
                               NULL,
                               &pgrest_doc_url_prefix,
                               "/doc/",
                               PGC_SUSET,
                               GUC_NOT_IN_SAMPLE,
                               NULL, NULL, NULL);

    DefineCustomStringVariable("pg_rest.sql_url_prefix",
                               gettext_noop("URL prefix used for SQL endpoint (user API)."),
                               NULL,
                               &pgrest_sql_url_prefix,
                               "/sql/",
                               PGC_SUSET,
                               GUC_NOT_IN_SAMPLE,
                               NULL, NULL, NULL);

    DefineCustomBoolVariable("pg_rest.https_enabled",
                             gettext_noop("Whether to enable listening for client requests on an HTTPS(SSL) port."),
                             NULL,
                             &pgrest_https_enabled,
                             true,
                             PGC_POSTMASTER,
                             GUC_NOT_IN_SAMPLE,
                             NULL, NULL, NULL);

    DefineCustomIntVariable("pg_rest.https_port",
                            gettext_noop("Network port to listen on for HTTPS(SSL) requests."),
                            NULL,
                            &pgrest_https_port,
                            8081,
                            1,
                            65535,
                            PGC_POSTMASTER,
                            GUC_NOT_IN_SAMPLE,
                            NULL, NULL, NULL);

    DefineCustomStringVariable("pg_rest.https_ssl_key",
                               gettext_noop("SSL key file to use for HTTPS (SSL) connections."),
                               NULL,
                               &pgrest_https_ssl_key,
                               "pg_rest/sslkey.pem",
                               PGC_USERSET,
                               GUC_NOT_IN_SAMPLE,
                               NULL, NULL, NULL);

    DefineCustomIntVariable("pg_rest.max_workers",
                            gettext_noop("Max number of pg_rest I/O processes."),
                            NULL,
                            &pgrest_max_workers,
                            4,
                            1,
                            INT_MAX,
                            PGC_POSTMASTER,
                            GUC_NOT_IN_SAMPLE,
                            NULL, NULL, NULL);
}


static void
pgrest_event_and_timer()
{

}


static void
pgrest_init_bg_workers()
{
    BackgroundWorker worker;
    int i;

    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    worker.bgw_main = pgrest_worker_main;
    worker.bgw_notify_pid = 0;

    for (i = 1; i <= pgrest_max_workers; i++) {
        snprintf(worker.bgw_name, BGW_MAXLEN, "pg_rest http worker");
        worker.bgw_main_arg = Int32GetDatum(i);

        RegisterBackgroundWorker(&worker);
	}
}

void
pgrest_worker_main(Datum main_arg)
{

    /* initialize subsystems XXX */



    /*
     * Main loop: do this until the SIGTERM handler tells us to terminate
     */
    for (;;) {
        ereport(DEBUG1,(errmsg("pg_rest http worker cycle")));

        pgrest_event_and_timer();

        /* postmaster died*/
        if (pgrest_postmaster_death) {
            ereport(LOG,(errmsg("pg_rest http worker postmaster died,exiting")));
            break;
        }

        /* got SIGTERM */
        if (pgrest_terminate) {
            ereport(LOG,(errmsg("pg_rest http worker got SIGTERM,exiting")));
            break;
        }

        if (pgrest_reconfigure) {
            pgrest_reconfigure = false;
            ereport(LOG,(errmsg("pg_rest http worker got SIGHUP,reconfiguring")));

            ProcessConfigFile(PGC_SIGHUP);
        }
    }

    proc_exit(1);
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
    if (!process_shared_preload_libraries_in_progress) {
        ereport(ERROR,
            (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
             errmsg("pg_rest must be loaded via shared_preload_libraries")));
    }

    /* initialize guc variables */
    pgrest_init_misc_guc();

    /* initialize shared memory XXX */

    /* initialize http workers*/
    pgrest_init_bg_workers();

}
