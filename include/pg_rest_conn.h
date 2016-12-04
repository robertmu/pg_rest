/*-------------------------------------------------------------------------
 *
 * include/pg_rest_conn.h
 *
 * 
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_CONN_H
#define PG_REST_CONN_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef enum {
    PGREST_TCP_NODELAY_UNSET = 0,
    PGREST_TCP_NODELAY_SET,
    PGREST_TCP_NODELAY_DISABLED
} pgrest_tcp_nodelay_e;


typedef enum {
    PGREST_TCP_NOPUSH_UNSET = 0,
    PGREST_TCP_NOPUSH_SET,
    PGREST_TCP_NOPUSH_DISABLED
} pgrest_tcp_nopush_e;


struct pgrest_connection_s {
    void                 *data;

    struct event         *rev;
    struct event         *wev;

    evutil_socket_t       fd;
    pgrest_listener_t    *listener;
    off_t                 sent;

    pgrest_mpool_t       *pool;

    int                   type;
    struct sockaddr      *sockaddr;
    socklen_t             socklen;
    char                 *addr_text;

#ifdef HAVE_OPENSSL
    /* TODO ssl stuff need to do */
#endif

    struct sockaddr      *local_sockaddr;
    socklen_t             local_socklen;

    pgrest_buffer_t      *buffer;

    /* list link in pgrest_conn_reuse_conns */
    dlist_node            elem;

    event_callback_fn     rev_handler;
    event_callback_fn     wev_handler;
    pgrest_uint_t         requests;

    unsigned              ready:1;
    unsigned              timedout:1;
    unsigned              error:1;
    unsigned              destroyed:1;

    unsigned              idle:1;
    unsigned              reusable:1;
    unsigned              close:1;
    unsigned              event:1;
    unsigned              channel:1;

    unsigned              tcp_nodelay:2;
    unsigned              tcp_nopush:2;
};

extern int                pgrest_conn_free_noconn;

void pgrest_conn_init(int worker_noconn);
pgrest_connection_t *pgrest_conn_get(evutil_socket_t fd);
void pgrest_conn_free(pgrest_connection_t *conn);
void pgrest_conn_close(pgrest_connection_t *conn);
void pgrest_conn_reusable(pgrest_connection_t *conn, int reusable);
ssize_t pgrest_conn_recv(pgrest_connection_t *conn, 
                         unsigned char *buf, 
                         size_t size);
ssize_t pgrest_conn_send(pgrest_connection_t *conn, 
                         unsigned char *buf, 
                         size_t size);
bool pgrest_conn_local_sockaddr(pgrest_connection_t *conn, 
                                pgrest_string_t *s,
                                bool port);
int  pgrest_conn_tcp_nopush(pgrest_connection_t *conn);
int  pgrest_conn_tcp_push(pgrest_connection_t *conn);

#endif /* PG_REST_CONN_H_ */
