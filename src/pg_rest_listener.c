/* -------------------------------------------------------------------------
 *
 * pq_rest_listener.c
 *   listener routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_listener.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

static List *pgrest_listener_listeners = NIL;

pgrest_listener_t *
pgrest_listener_create(void *sockaddr, socklen_t socklen)
{
    pgrest_listener_t   *listener;
    struct sockaddr     *sa;
    char                 text[PGREST_SOCKADDR_STRLEN];

    listener = (pgrest_listener_t *) palloc0fast(sizeof(pgrest_listener_t));

    sa = (struct sockaddr *) palloc(socklen);
    memcpy(sa, sockaddr, socklen);

    listener->sockaddr = sa;
    listener->socklen = socklen;

    (void) pgrest_inet_ntop(sa, socklen, text, PGREST_SOCKADDR_STRLEN, true);
    listener->addr_text = pstrdup(text);

    switch (listener->sockaddr->sa_family) {
#ifdef HAVE_IPV6
    case AF_INET6:
        listener->addr_text_max_len = PGREST_INET6_ADDRSTRLEN;
        break;
#endif
#if HAVE_UNIX_SOCKETS
    case AF_UNIX:
        listener->addr_text_max_len = PGREST_UNIX_ADDRSTRLEN;
        break;
#endif
    case AF_INET:
        listener->addr_text_max_len = PGREST_INET_ADDRSTRLEN;
        break;
    default:
        listener->addr_text_max_len = PGREST_SOCKADDR_STRLEN;
        break;
    }

    listener->fd = (evutil_socket_t) -1;
    listener->type = SOCK_STREAM;
    listener->backlog = DEFAUlT_PGREST_LISTEN_BACKLOG;
    listener->rcvbuf = -1;
    listener->sndbuf = -1;

#ifdef HAVE_SETFIB
    listener->setfib = -1;
#endif
#ifdef HAVE_TCP_FASTOPEN
    listener->fastopen = -1;
#endif
    pgrest_listener_listeners = lappend(pgrest_listener_listeners, listener);

    return listener;
}

static bool
pgrest_listener_open(List *listeners)
{
    int                reuseaddr = 1;
    evutil_socket_t    fd;
    ListCell          *cell;

    /* for each listening socket */
    foreach(cell, listeners) {
        pgrest_listener_t *listener = lfirst(cell);

        if (listener->fd != (evutil_socket_t) -1) {
            continue;
        }

        fd = socket(listener->sockaddr->sa_family, listener->type, 0);

        if (fd == (evutil_socket_t) -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "could not create %s socket: %m",
                            listener->addr_text)));
            return false;
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                       (const void *) &reuseaddr, sizeof(int)) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "setsockopt(SO_REUSEADDR) "
                            "for %s failed: %m", listener->addr_text)));
            evutil_closesocket(fd);
            return false;
        }

#ifdef HAVE_REUSEPORT
        if (listener->reuseport) {
            int reuseport = 1;

            if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                           (const void *) &reuseport, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_REUSEPORT) "
                                "for %s failed: %m", listener->addr_text)));
                evutil_closesocket(fd);
                return false;
            }
        }
#endif

#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
        if (listener->sockaddr->sa_family == AF_INET6) {
            int ipv6only = listener->ipv6only;

            if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                           (const void *) &ipv6only, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(IPV6_V6ONLY) "
                                "for %s failed: %m,ignored", listener->addr_text)));
            }
        }
#endif

        if (evutil_make_socket_nonblocking(fd) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "make socket nonblocking "
                            "for %s failed: %m", listener->addr_text)));
            evutil_closesocket(fd);
            return false;
        }

        if (bind(fd, listener->sockaddr, listener->socklen) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "could not bind to %s failed: %m",
                            listener->addr_text),
                     (IS_AF_UNIX(listener->sockaddr->sa_family)) ?
                  errhint("Is another pg_rest already running on port %s? "
                          "If not, remove socket file \"%s\" and retry.",
                          listener->addr_text, listener->addr_text) :
                  errhint("Is another pg_rest already running on port %s? "
                          "If not, wait a few seconds and retry.",
                          listener->addr_text)));
            evutil_closesocket(fd);
            return false;
        }

#ifdef HAVE_UNIX_SOCKETS
        if (listener->sockaddr->sa_family == AF_UNIX) {
            mode_t   mode;
            mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

            if (chmod(listener->addr_text, mode) == -1) {
                ereport(LOG,
                        (errcode_for_file_access(),
                         errmsg(PGREST_PACKAGE " " "could not set permissions of"
                                " file \"%s\": %m", listener->addr_text)));
            }
        }
#endif

        if (listen(fd, listener->backlog) == -1) {
            ereport(LOG,
                    (errcode_for_socket_access(),
                     errmsg(PGREST_PACKAGE " " "could not listen on %s, "
                            "backlog %d : %m", listener->addr_text, listener->backlog)));
            evutil_closesocket(fd);
            return false;
        }

        listener->listen = 1;
        listener->fd = fd;
    }

    return true;
}

static void
pgrest_listener_configure(List *listeners)
{
    int                        value;
    ListCell                  *cell;
#if defined(HAVE_DEFERRED_ACCEPT) && defined(SO_ACCEPTFILTER)
    struct accept_filter_arg   af;
#endif

    /* for each listening socket */
    foreach(cell, listeners) {
        pgrest_listener_t *listener = lfirst(cell);

        if (listener->rcvbuf != -1) {
            if (setsockopt(listener->fd, SOL_SOCKET, SO_RCVBUF,
                           (const void *) &listener->rcvbuf, sizeof(int)) == -1 ) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_RCVBUF, %d) "
                                "for %s failed : %m ignored",
                         listener->rcvbuf, listener->addr_text)));
            }
        }

        if (listener->sndbuf != -1) {
            if (setsockopt(listener->fd, SOL_SOCKET, SO_SNDBUF,
                           (const void *) &listener->sndbuf, sizeof(int)) == -1 ) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_SNDBUF, %d) "
                                "for %s failed : %m ignored",
                         listener->sndbuf, listener->addr_text)));
            }
        }

        if (listener->keepalive) {
            value = (listener->keepalive == 1) ? 1 : 0;

            if (setsockopt(listener->fd, SOL_SOCKET, SO_KEEPALIVE,
                           (const void *) &value, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_KEEPALIVE, %d) "
                                "for %s failed : %m ignored",
                         value, listener->addr_text)));
            }
        }

#ifdef HAVE_KEEPALIVE_TUNABLE
        if (listener->keepidle) {
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_KEEPIDLE,
                           (const void *) &listener->keepidle, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_KEEPIDLE, %d) "
                                "for %s failed : %m ignored",
                         listener->keepidle, listener->addr_text)));
            }
        }

        if (listener->keepintvl) {
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_KEEPINTVL,
                           (const void *) &listener->keepintvl, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_KEEPINTVL, %d) "
                                "for %s failed : %m ignored",
                         listener->keepintvl, listener->addr_text)));
            }
        }

        if (listener->keepcnt) {
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_KEEPCNT,
                           (const void *) &listener->keepcnt, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_KEEPCNT, %d) "
                                "for %s failed : %m ignored",
                         listener->keepcnt, listener->addr_text)));
            }
        }
#endif

#ifdef HAVE_SETFIB
        if (listener->setfib != -1) {
            if (setsockopt(listener->fd, SOL_SOCKET, SO_SETFIB,
                           (const void *) &listener->setfib, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_SETFIB, %d) "
                                "for %s failed : %m ignored",
                         listener->setfib, listener->addr_text)));
            }
        }
#endif

#ifdef HAVE_TCP_FASTOPEN
        if (listener->fastopen != -1) {
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_FASTOPEN,
                           (const void *) &listener->fastopen, sizeof(int)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_FASTOPEN, %d) "
                                "for %s failed : %m ignored",
                         listener->fastopen, listener->addr_text)));
            }
        }
#endif

#ifdef HAVE_DEFERRED_ACCEPT

#ifdef SO_ACCEPTFILTER
        if (listener->add_deferred) {
            MemSet(&af, 0, sizeof(accept_filter_arg));
            StrNCpy(af.af_name, listener->accept_filter, PGREST_AF_SIZE);

            if (setsockopt(listener->fd, SOL_SOCKET, SO_ACCEPTFILTER,
                           &af, sizeof(struct accept_filter_arg)) == -1) {
                ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(SO_ACCEPTFILTER, \"%s\") "
                                "for %s failed : %m ignored",
                         listener->accept_filter, listener->addr_text)));
                continue;
            }

            listener->deferred_accept = 1;
        }
#endif

#ifdef TCP_DEFER_ACCEPT
        if (listener->add_deferred) {
            value = 1;
            if (setsockopt(listener->fd, IPPROTO_TCP, TCP_DEFER_ACCEPT,
                           &value, sizeof(int)) == -1) {
                 ereport(LOG,
                        (errcode_for_socket_access(),
                         errmsg(PGREST_PACKAGE " " "setsockopt(TCP_DEFER_ACCEPT, %d) "
                                "for %s failed : %m ignored",
                         value, listener->addr_text)));
                continue;
            }
        }

        if (listener->add_deferred) {
            listener->deferred_accept = 1;
        }
#endif

#endif /* HAVE_DEFERRED_ACCEPT */
    }
}

static void
pgrest_listener_close(List *listeners)
{
    ListCell            *cell;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;

    pgrest_acceptor_use_mutex = false;
    pgrest_acceptor_mutex_held = false;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "listener close")));

    foreach(cell, listeners) {
        listener = lfirst(cell);
        conn = listener->connection;

        if (conn) {
            /* worker only  */
            (void) pgrest_event_del(conn->rev, EV_READ);
            pgrest_conn_free(conn);
            conn->fd = (evutil_socket_t) -1;
        }

        ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "%s close listener %s %d", 
                                !IsUnderPostmaster ? "postmaster" : "worker", 
                                listener->addr_text, 
                                listener->fd)));

        closesocket(listener->fd);

#ifdef HAVE_UNIX_SOCKETS
        if (listener->sockaddr->sa_family == AF_UNIX && !IsUnderPostmaster) {
            (void) unlink(listener->addr_text);
            debug_log(DEBUG1,(errmsg(PGREST_PACKAGE " " "unlink unix domain socket")));
        }
#endif
        listener->fd = (evutil_socket_t) -1;
    }
}

bool
pgrest_listener_resume(void)
{
    ListCell            *cell;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;
    struct event        *ev;

    foreach(cell, pgrest_listener_listeners) {
        listener = lfirst(cell);
        conn = listener->connection;
        ev = conn->rev;

        if (conn == NULL || ev == NULL) {
            continue;
        }

        if(!pgrest_event_add(ev, EV_READ, 0)) {
            return false;
        }
    }

    return true;
}

bool
pgrest_listener_pause(bool all)
{
    ListCell            *cell;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;
    struct event        *ev;

    foreach(cell, pgrest_listener_listeners) {
        listener = lfirst(cell);
        conn = listener->connection;
        ev = conn->rev;

        if (conn == NULL || ev == NULL) {
            continue;
        }
#ifdef HAVE_REUSEPORT
        if (listener->reuseport && !all) {
            continue;
        }
#endif

        if (!pgrest_event_del(ev, EV_READ)) {
            return false;
        }
    }

    return true;
}

static bool
pgrest_listener_worker_init(void *data, void *base)
{
    ListCell            *cell;
    pgrest_listener_t   *listener;
    pgrest_connection_t *conn;
    List                *listeners = data;

    /* for each listening socket */
    foreach(cell, listeners) {
        listener = lfirst(cell);

        conn = pgrest_conn_get(listener->fd);
        if (conn == NULL) {
            return false;
        }

        conn->type = listener->type;
        conn->listener = listener;

        listener->connection = conn;
        if (pgrest_event_assign(conn->rev, base, 
                                conn->fd, EV_READ | EV_PERSIST, 
                                pgrest_acceptor_accept_handler, conn) < 0) 
        {
            return false;
        }

        pgrest_event_priority_set(conn->rev, 0);
#ifdef HAVE_REUSEPORT
        if (listener->reuseport) {
            if (!pgrest_event_add(conn->rev, EV_READ, 0)) {
                return false;
            }

            continue;
        }
#endif
        if (pgrest_acceptor_use_mutex) {
            continue;
        }

        if (!pgrest_event_add(conn->rev, EV_READ, 0)) {
            return false;
        }
    }

    return true;
}

static bool
pgrest_listener_worker_exit(void *data, void *base)
{
    List       *listeners = data;

    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "worker %d listener_exit",
                               pgrest_worker_index)));

    pgrest_listener_close(listeners);
    return true;
}

static StringInfo
pgrest_listener_info_format(List *listeners)
{
    StringInfo             result;
    ListCell              *cell;
    pgrest_listener_t     *listener;

    result = makeStringInfo();

    /* for each listening socket */
    foreach(cell, listeners) {
        listener = lfirst(cell);

        appendStringInfo(result,"\n");

        appendStringInfo(result,
            "!\tlistener addr:%s\n", listener->addr_text);
        appendStringInfo(result,
            "!\tlistener open:%d\n", listener->open);
#if (HAVE_REUSEPORT)
        appendStringInfo(result,
            "!\tlistener reuseport:%d\n", listener->reuseport);
#endif

#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
        appendStringInfo(result,
            "!\tlistener addr:%d\n", listener->ipv6only);
#endif

        appendStringInfo(result,
            "!\tlistener listen:%d\n", listener->listen);
        appendStringInfo(result,
            "!\tlistener post_accept_timeout:%d\n", listener->post_accept_timeout);
        appendStringInfo(result,
            "!\tlistener backlog:%d\n", listener->backlog);
        appendStringInfo(result,
            "!\tlistener sndbuf:%d\n", listener->sndbuf);
        appendStringInfo(result,
            "!\tlistener keepalive:%d\n", listener->keepalive);
        appendStringInfo(result,
            "!\tlistener rcvbuf:%d\n", listener->rcvbuf);

#if defined(HAVE_DEFERRED_ACCEPT) && defined(SO_ACCEPTFILTER)
        appendStringInfo(result,
            "!\tlistener accept_filter:%s\n", listener->accept_filter);
#endif
#if defined(HAVE_DEFERRED_ACCEPT) && defined(TCP_DEFER_ACCEPT)
        appendStringInfo(result,
            "!\tlistener deferred_accept:%d\n", listener->deferred_accept);
#endif
#ifdef HAVE_SETFIB
        appendStringInfo(result,
            "!\tlistener setfib:%d\n", listener->setfib);
#endif
#ifdef HAVE_TCP_FASTOPEN
        appendStringInfo(result,
            "!\tlistener fastopen:%d\n", listener->fastopen);
#endif

    }

    return result;
}

void
pgrest_listener_info_print(void)
{
    StringInfo buffer;

    buffer = pgrest_listener_info_format(pgrest_listener_listeners);

    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print %s listener ++++++++++", 
                            !IsUnderPostmaster ? "postmaster" : "worker")));
    ereport(DEBUG1,(errmsg("\n%s", buffer->data)));
    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print %s listener ----------", 
                            !IsUnderPostmaster ? "postmaster" : "worker")));

    pfree(buffer->data);
    pfree(buffer);
}

static void
pgrest_listener_fini2(int status, Datum arg)
{
    pgrest_listener_fini();
}

void
pgrest_listener_init(void)
{
    if (pgrest_listener_listeners == NIL) {
        ereport(FATAL,
                (errmsg(PGREST_PACKAGE " " "could not create any listener")));
    }

    if (pgrest_listener_open(pgrest_listener_listeners) == false) {
        ereport(FATAL,
                (errmsg(PGREST_PACKAGE " " "failed to open socket listener")));
    }

    pgrest_listener_configure(pgrest_listener_listeners);

    pgrest_worker_hook_add(pgrest_listener_worker_init, 
                           pgrest_listener_worker_exit, 
                           "listener", 
                           pgrest_listener_listeners, 
                           false);

    on_proc_exit(pgrest_listener_fini2, 0);
#if PGREST_DEBUG
    pgrest_listener_info_print();
#endif
}

void
pgrest_listener_fini(void)
{
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "listener fini")));
    pgrest_listener_close(pgrest_listener_listeners);
}
