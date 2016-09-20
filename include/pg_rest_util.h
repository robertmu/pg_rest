/*-------------------------------------------------------------------------
 *
 * include/pg_rest_util.h
 *
 * misc routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_UTIL_H
#define PG_REST_UTIL_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

/* True iff e is an error that means a read/write operation can be retried. */
#define PGREST_UTIL_ERR_RW_RETRIABLE(e)             \
    ((e) == EAGAIN || (e) == EWOULDBLOCK || (e) == EINTR)

#if PGREST_DEBUG
#define debug_log(elevel, rest)	     ereport(elevel, rest)
#else
#define debug_log(elevel, rest)
#endif

#if PGREST_MM_REPLACE
#define PGREST_UTIL_SWITCH_CTX(old_ctx, new_ctx)    \
    old_ctx = MemoryContextSwitchTo(new_ctx)
#define PGREST_UTIL_RESTORE_CTX(old_ctx)            \
    MemoryContextSwitchTo(old_ctx)
#else
#define PGREST_UTIL_SWITCH_CTX(old_ctx, new_ctx) 
#define PGREST_UTIL_RESTORE_CTX(old_ctx) 
#endif

#if PGSQL_VERSION < 96

/*
 * Recommended alloc parameters for "small" contexts that are never expected
 * to contain much data (for example, a context to contain a query plan).
 */
#define ALLOCSET_SMALL_MINSIZE	 0
#define ALLOCSET_SMALL_INITSIZE  (1 * 1024)
#define ALLOCSET_SMALL_MAXSIZE	 (8 * 1024)
#define ALLOCSET_SMALL_SIZES                        \
    ALLOCSET_SMALL_MINSIZE, ALLOCSET_SMALL_INITSIZE, ALLOCSET_SMALL_MAXSIZE

/*
 * Recommended default alloc parameters, suitable for "ordinary" contexts
 * that might hold quite a lot of data.
 */
#define ALLOCSET_DEFAULT_MINSIZE   0
#define ALLOCSET_DEFAULT_INITSIZE  (8 * 1024)
#define ALLOCSET_DEFAULT_MAXSIZE   (8 * 1024 * 1024)
#define ALLOCSET_DEFAULT_SIZES \
	ALLOCSET_DEFAULT_MINSIZE, ALLOCSET_DEFAULT_INITSIZE, ALLOCSET_DEFAULT_MAXSIZE

#endif

const char *pgrest_util_inet_ntop(struct sockaddr *sa, 
                                  socklen_t socklen,
                                  char *dst, size_t size);
void *pgrest_util_palloc(Size size);
void *pgrest_util_repalloc(void *pointer, Size size);
void  pgrest_util_pfree(void *pointer);
void  pgrest_util_event_log(int severity, const char *msg);
void  pgrest_util_event_fatal(int err);

#endif /* PG_REST_UTIL_H */
