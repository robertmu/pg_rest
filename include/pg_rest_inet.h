/*-------------------------------------------------------------------------
 *
 * include/pg_rest_inet.h
 *
 * inet routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_INET_H
#define PG_REST_INET_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

#define PGREST_INET_ADDRSTRLEN   (sizeof("255.255.255.255"))
#define PGREST_INET6_ADDRSTRLEN                                                 \
    (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"))
#define PGREST_UNIX_ADDRSTRLEN                                                  \
    (sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

#if HAVE_UNIX_SOCKETS
#define PGREST_SOCKADDR_STRLEN   (sizeof("unix:") - 1 + PGREST_UNIX_ADDRSTRLEN)
#elif HAVE_IPV6
#define PGREST_SOCKADDR_STRLEN   (PGREST_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1)
#else
#define PGREST_SOCKADDR_STRLEN   (PGREST_INET_ADDRSTRLEN + sizeof(":65535") - 1)
#endif

typedef union {
    struct sockaddr           sockaddr;
    struct sockaddr_in        sockaddr_in;
#ifdef HAVE_IPV6
    struct sockaddr_in6       sockaddr_in6;
#endif
#ifdef HAVE_UNIX_SOCKETS
    struct sockaddr_un        sockaddr_un;
#endif
} pgrest_sockaddr_t;

typedef struct {
    struct sockaddr          *sockaddr;
    socklen_t                 socklen;
    char                     *name;
    size_t                    name_len;
} pgrest_addr_t;

typedef struct {
    char                     *url;
    size_t                    url_len;
    char                     *host;
    size_t                    host_len;
    char                     *port_text;
    size_t                    port_text_len;

    in_port_t                 port;
    in_port_t                 default_port;
    int                       family;

    unsigned                  listen:1;
    unsigned                  no_resolve:1;

    unsigned                  no_port:1;
    unsigned                  wildcard:1;

    socklen_t                 socklen;
    pgrest_sockaddr_t         sockaddr;

    pgrest_addr_t            *addrs;
    pgrest_uint_t             naddrs;

    char                     *err;
} pgrest_url_t;

const char *pgrest_inet_ntop(struct sockaddr *sa, 
                             socklen_t socklen,
                             char *dst, 
                             size_t size, 
                             bool port);
bool  pgrest_inet_cmp_sockaddr(struct sockaddr *sa1, 
                             socklen_t slen1,
                             struct sockaddr *sa2, 
                             socklen_t slen2, 
                             bool cmp_port);
in_port_t pgrest_inet_get_port(struct sockaddr *sa);
void  pgrest_inet_set_port(struct sockaddr *sa, in_port_t port);
in_addr_t pgrest_inet_inet_addr(unsigned char *text, size_t len);
bool  pgrest_inet_parse_url(pgrest_url_t *url);
#ifdef HAVE_IPV6
bool  pgrest_inet_inet6_addr(unsigned char *p, 
                             size_t len, 
                             unsigned char *addr);
#endif
bool  pgrest_inet_resolve_host(pgrest_url_t *u);

#endif /* PG_REST_INET_H */
