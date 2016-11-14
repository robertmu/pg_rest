/*-------------------------------------------------------------------------
 *
 * include/pg_rest_event.h
 *
 * wrapper of libevent
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_EVENT_H
#define PG_REST_EVENT_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

/* FIXME Maybe we need to modify libevent */
#define pgrest_event_dispatch(b)     event_base_loop(b, EVLOOP_ONCE)
#define pgrest_event_run_pending(b)  

static inline int 
pgrest_event_assign(struct event *ev, struct event_base *base, 
                    evutil_socket_t fd, short events,
                    event_callback_fn callback, void *arg)
{
    return event_assign(ev, base, fd, events, callback, arg);
}

static inline void
pgrest_event_loop_once(struct event_base *base)
{
    if (pgrest_event_dispatch(base) == -1) {
        ereport(LOG,
                (errmsg(PGREST_PACKAGE " " "event_dispatch() failed: %m")));
    }
}

bool  pgrest_event_add(struct event *ev, short events, int timeout);
bool  pgrest_event_del(struct event *ev, short events);
struct event *pgrest_event_new(struct event_base *base, 
                               evutil_socket_t fd, 
                               short events, 
                               event_callback_fn handler, 
                               void *arg);
int   pgrest_event_priority_set(struct event *ev, int pri);
void  pgrest_event_free(struct event *ev);
int   pgrest_event_get_signal(const struct event *ev);
void *pgrest_event_self_cbarg(void);
struct event_base *pgrest_event_base_new(void);
int   pgrest_event_base_priority_init(struct event_base *base, int n);
void  pgrest_event_base_free(struct event_base *base);
void  pgrest_event_global_shutdown(void);
void  pgrest_event_log(int severity, const char *msg);
void  pgrest_event_fatal(int err);

#endif /* PG_REST_EVENT_H */
