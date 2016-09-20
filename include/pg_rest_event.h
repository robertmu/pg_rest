/*-------------------------------------------------------------------------
 *
 * include/pg_rest_event.h
 *
 * wrapper of libevent(for MemoryContext use)
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_EVENT_H
#define PG_REST_EVENT_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

static inline struct bufferevent *
pgrest_bufferevent_socket_new(struct event_base *base, evutil_socket_t fd, int options)
{
    struct bufferevent *be;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    be = bufferevent_socket_new(base, fd, options);
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return be;
}

static inline void 
pgrest_bufferevent_free(struct bufferevent *bufev)
{
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    bufferevent_free(bufev);
    PGREST_UTIL_RESTORE_CTX(old_ctx);
}

static inline int 
pgrest_bufferevent_write(struct bufferevent *bufev, const void *data, size_t size)
{
    int                 result;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = bufferevent_write(bufev, data, size);
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}

static inline void 
pgrest_bufferevent_setcb(struct bufferevent *bufev,
                         bufferevent_data_cb readcb, 
                         bufferevent_data_cb writecb,
                         bufferevent_event_cb eventcb, 
                         void *cbarg)
{
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    bufferevent_setcb(bufev, readcb, writecb, eventcb, cbarg);
    PGREST_UTIL_RESTORE_CTX(old_ctx);
}

static inline int 
pgrest_bufferevent_enable(struct bufferevent *bufev, short event)
{
    int                 result;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = bufferevent_enable(bufev, event);
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}

static inline size_t 
pgrest_bufferevent_read(struct bufferevent *bufev, void *data, size_t size)
{
    size_t              result;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif

    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = bufferevent_read(bufev, data, size);
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}

static inline int 
pgrest_event_base_loop(struct event_base *base, int flag)
{
    int                 result;
#if PGREST_MM_REPLACE
    MemoryContext       old_ctx;
#endif
    PGREST_UTIL_SWITCH_CTX(old_ctx, pgrest_worker_event_context);
    result = event_base_loop(base, flag);
    PGREST_UTIL_RESTORE_CTX(old_ctx);

    return result;
}


bool  pgrest_event_add(struct event *ev, short events, int timeout);
bool  pgrest_event_del(struct event *ev, short events);
struct event *pgrest_event_new(struct event_base *base, 
                                    evutil_socket_t fd, 
                                    short events, 
                                    event_callback_fn handler, 
                                    void *arg);
int pgrest_event_priority_set(struct event *ev, int pri);
void pgrest_event_free(struct event *ev);
int pgrest_event_get_signal(const struct event *ev);
void *pgrest_event_self_cbarg(void);
struct event_base *pgrest_event_base_new(void);
int pgrest_event_base_priority_init(struct event_base *base, int npriorities);
void pgrest_event_base_free(struct event_base *base);
void pgrest_event_global_shutdown(void);

#endif /* PG_REST_EVENT_H */
