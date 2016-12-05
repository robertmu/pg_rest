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
    struct timeval tv;

    tv.tv_sec = (long) (timeout/ 1000);
    tv.tv_usec = (long) ((timeout % 1000) * 1000);

    if (event_pending(ev, events, NULL)) {
        return true;
    }

    if (event_add(ev, timeout ? &tv : NULL) == -1) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "error from"
                                 " event when adding event")));
        return false;
    }

    return true;
}

bool
pgrest_event_del(struct event *ev, short events)
{
    if (!event_pending(ev, events, NULL)) {
        return true;
    }

    if (event_del(ev) == -1) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "error from"
                                  " event when delete event")));
        return false;
    }

    return true;
}

struct event *
pgrest_event_new(struct event_base *base, 
                 evutil_socket_t fd, 
                 short events, 
                 event_callback_fn handler, 
                 void *arg)
{
    return event_new(base, fd, events, handler, arg); 
}

int 
pgrest_event_priority_set(struct event *ev, int pri)
{
    return event_priority_set(ev, pri);
}

void 
pgrest_event_free(struct event *ev)
{
    event_free(ev);
}

int pgrest_event_get_signal(const struct event *ev)
{
    return event_get_signal(ev);
}

struct event_base *
pgrest_event_base_new(void)
{
    return event_base_new();
}

int 
pgrest_event_base_priority_init(struct event_base *base, int npriorities)
{
    return event_base_priority_init(base, npriorities);
}

void 
pgrest_event_base_free(struct event_base *base)
{
    event_base_free(base);
}

#if 0
void 
pgrest_event_global_shutdown(void)
{
    libevent_global_shutdown();
}
#endif

void  
pgrest_event_log(int severity, const char *msg)
{
    int         level;

    switch (severity) {
    case _EVENT_LOG_WARN:
        level = WARNING;
        break;
    case _EVENT_LOG_ERR:
        level = WARNING;
        break;
    default:
        level = 0;
    }

    if (level) {
        ereport(level, (errmsg(PGREST_PACKAGE " " "worker %d event internal"
                                " error: %s", pgrest_worker_index, msg)));
    }
}

void  
pgrest_event_fatal(int err)
{
    ereport(ERROR, (errmsg(PGREST_PACKAGE " " "worker %d event fatal"
                           " error: %d", pgrest_worker_index, err)));
}
