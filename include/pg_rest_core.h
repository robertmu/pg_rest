/*-------------------------------------------------------------------------
 *
 * include/pg_rest_core.h
 *
 * 
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_CORE_H
#define PG_REST_CORE_H

#include "pg_rest_config.h"

typedef struct pgrest_connection_s        pgrest_connection_t;
typedef struct pgrest_settings_s          pgrest_settings_t;
typedef void   (*pgrest_conn_handler_pt)  (pgrest_connection_t *conn);

#define  PGREST_OK          0
#define  PGREST_ERROR      -1
#define  PGREST_AGAIN      -2

#include <miscadmin.h>
#include <storage/ipc.h>
#include <storage/lwlock.h>
#include <storage/proc.h>
#include <storage/shmem.h>
#include <storage/spin.h>
#include <fmgr.h>
#include <utils/builtins.h>
#include <tcop/utility.h>
#include <pgstat.h>
#include <libpq/ip.h>
#include <postmaster/bgworker.h>
#include <lib/ilist.h>
#include <lib/stringinfo.h>
#include <utils/memutils.h>
#include <utils/guc_tables.h>

#if PGSQL_VERSION >= 95
#include <port/atomics.h>
#endif

#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifndef WIN32
#include <sys/resource.h>
#endif

#include <event2/event.h>
#include <event2/bufferevent.h>
#ifdef HAVE_OPENSSL
#include <event2/bufferevent_ssl.h>
#endif

#include <jansson.h>

#include "pg_rest_util.h"
#include "pg_rest_slock.h"
#include "pg_rest_array.h"
#include "pg_rest_cqueue.h"
#include "pg_rest_guc.h"
#include "pg_rest_conf.h"
#include "pg_rest_slab.h"
#include "pg_rest_ipc.h"
#include "pg_rest_shm.h"
#include "pg_rest_worker.h"
#include "pg_rest_event.h"

#include "pg_rest_listener.h"
#include "pg_rest_acceptor.h"
#include "pg_rest_conn.h"

#endif /* PG_REST_CORE_H */
