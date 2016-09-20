/* -------------------------------------------------------------------------
 *
 * pq_rest_event.c
 *   wrapper of libevent(for MemoryContext use)
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_event.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

bool
pgrest_event_add(struct event *ev, short events, int timeout)
{
    struct timeval     tv;
#if PGREST_MM_REPLACE
    MemoryContext      old_ctx;
#endif

    tv.tv_sec = (long) (timeout/ 1000);
    tv.tv_usec = (long) ((timeout % 1000) * 1000);

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);

    if (event_pending(ev, events, NULL)) {
        PGREST_UTIL_RESTORE_CTX(old_ctx);
        return true;
    }

    if (event_add(ev, timeout ? &tv : NULL) == -1) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "error from event "
                                 "when adding event")));
        PGREST_UTIL_RESTORE_CTX(old_ctx);
        return false;
    }

    PGREST_UTIL_RESTORE_CTX(old_ctx);
    return true;
}

bool
pgrest_event_del(struct event *ev, short events)
{
#if PGREST_MM_REPLACE
    MemoryContext      old_ctx;
#endif

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);

    if (! event_pending(ev, events, NULL)) {
        PGREST_UTIL_RESTORE_CTX(old_ctx);
        return true;
    }

    if (event_del(ev) == -1) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "error from event "
                               "when delete event")));
        PGREST_UTIL_RESTORE_CTX(old_ctx);
        return false;
    }

    PGREST_UTIL_RESTORE_CTX(old_ctx);
    return true;
}

struct event *
pgrest_event_new(struct event_base *base, 
                      evutil_socket_t fd, 
                      short events, 
                      event_callback_fn handler, 
                      void *arg)
{
    struct event      *ev;
#if PGREST_MM_REPLACE
    MemoryContext      old_ctx;
#endif

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    ev = event_new(base, fd, events, handler, arg); 
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return ev;
}

int 
pgrest_event_priority_set(struct event *ev, int pri)
{
    int                result;
#if PGREST_MM_REPLACE
    MemoryContext      old_ctx;
#endif

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = event_priority_set(ev, pri);
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}

void 
pgrest_event_free(struct event *ev)
{
#if PGREST_MM_REPLACE
    MemoryContext      old_ctx;
#endif

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    event_free(ev);
    PGREST_UTIL_RESTORE_CTX(old_ctx);
}

int pgrest_event_get_signal(const struct event *ev)
{
    int                 result;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = event_get_signal(ev);
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}

void *
pgrest_event_self_cbarg(void)
{
    void               *result;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = event_self_cbarg();
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}

struct event_base *
pgrest_event_base_new(void)
{
    void               *result;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = event_base_new();
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}

int 
pgrest_event_base_priority_init(struct event_base *base, int npriorities)
{
    int                 result;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = event_base_priority_init(base, npriorities);
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}

void 
pgrest_event_base_free(struct event_base *base)
{
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    event_base_free(base);
    PGREST_UTIL_RESTORE_CTX(old_ctx);
}

void 
pgrest_event_global_shutdown(void)
{
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    libevent_global_shutdown();
    PGREST_UTIL_RESTORE_CTX(old_ctx);
}
