/*-------------------------------------------------------------------------
 *
 * include/pg_rest_listener.h
 *
 * listener routines
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_LISTENER_H
#define PG_REST_LISTENER_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

/* Configure the UNIX socket location for the well known port. */

#define DEFAUlT_PGREST_LISTEN_BACKLOG          512
#define DEFAULT_PGREST_SOCKET_DIR              "/var/run"
#define PGREST_UNIXSOCK_PATH(path, port, sockdir)               \
        snprintf(path, sizeof(path), "%s/.s.PGREST.%d",         \
                ((sockdir) && *(sockdir) != '\0') ? (sockdir) : \
                DEFAULT_PGSOCKET_DIR,                           \
                (port))

typedef struct pgrest_listener_s  pgrest_listener_t;

struct pgrest_listener_s {
    evutil_socket_t          fd;

    struct sockaddr         *sockaddr;
    socklen_t                socklen;
    char                    *addr_text;
    size_t                   addr_text_max_len;

    int                      type;

    int                      backlog;
    int                      rcvbuf;
    int                      sndbuf;
#ifdef HAVE_KEEPALIVE_TUNABLE
    int                      keepidle;
    int                      keepintvl;
    int                      keepcnt;
#endif

    pgrest_conn_handler_pt   handler;
    void                    *servers; 

    pgrest_connection_t     *connection;

    int                      post_accept_timeout;

    unsigned                 open:1;
    unsigned                 bound:1;
    unsigned                 nonblocking_accept:1;
    unsigned                 listen:1;
    unsigned                 nonblocking:1;
    unsigned                 addr_ntop:1;
#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
    unsigned                 ipv6only:1;
#endif
#ifdef HAVE_REUSEPORT
    unsigned                 reuseport:1;
#endif
    unsigned                 keepalive:2;

#ifdef HAVE_DEFERRED_ACCEPT
    unsigned                 deferred_accept:1;
    unsigned                 add_deferred:1;
#ifdef SO_ACCEPTFILTER
    char                    *accept_filter;
#endif
#endif

#ifdef HAVE_SETFIB
    int                      setfib;
#endif
#ifdef HAVE_TCP_FASTOPEN
    int                      fastopen;
#endif
};

void  pgrest_listener_init(void);
bool  pgrest_listener_resume(void);
bool  pgrest_listener_pause(bool all);
void  pgrest_listener_info_print(void);
void  pgrest_listener_fini(void);
pgrest_listener_t* pgrest_listener_create(void *sockaddr, 
                                          socklen_t socklen);

#endif /* PG_REST_LISTENER_H_ */
