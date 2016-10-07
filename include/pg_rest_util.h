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
    char                 *url;
    size_t                url_len;
    char                 *host;
    size_t                host_len;
    char                 *port_text;
    size_t                port_text_len;

    in_port_t             port;
    in_port_t             default_port;
    int                   family;

    unsigned              listen:1;
    unsigned              no_resolve:1;

    unsigned              no_port:1;
    unsigned              wildcard:1;

    socklen_t             socklen;
    pgrest_sockaddr_t     sockaddr;

    pgrest_addr_t        *addrs;
    pgrest_uint_t         naddrs;

    char                 *err;
} pgrest_url_t;

const char *pgrest_util_inet_ntop(struct sockaddr *sa, 
                                  socklen_t socklen,
                                  char *dst, size_t size, 
                                  bool port);
void *pgrest_util_palloc(Size size);
void *pgrest_util_repalloc(void *pointer, Size size);
void  pgrest_util_pfree(void *pointer);
void  pgrest_util_event_log(int severity, const char *msg);
void  pgrest_util_event_fatal(int err);
bool  pgrest_util_cmp_sockaddr(struct sockaddr *sa1, socklen_t slen1,
                struct sockaddr *sa2, socklen_t slen2, bool cmp_port);
in_port_t pgrest_util_get_port(struct sockaddr *sa);
void  pgrest_util_set_port(struct sockaddr *sa, in_port_t port);
void  pgrest_util_sort(void *base, size_t n, size_t size,
                       int (*cmp)(const void *, const void *));
in_addr_t pgrest_util_inet_addr(unsigned char *text, size_t len);
bool pgrest_util_parse_url(pgrest_url_t *url);
#ifdef HAVE_IPV6
bool pgrest_util_inet6_addr(unsigned char *p, size_t len, unsigned char *addr);
#endif
bool pgrest_util_resolve_host(pgrest_url_t *u);


#endif /* PG_REST_UTIL_H */
