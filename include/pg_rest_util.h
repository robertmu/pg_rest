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
#define debug_log(elevel, rest)            ereport(elevel, rest)
#else
#define debug_log(elevel, rest)
#endif

#define pgrest_util_alloc(mctx, size)      pgrest_util_alloc_(mctx, size)
#define pgrest_util_calloc(mctx, n, size)  pgrest_util_calloc_(mctx, n, size)
#define pgrest_util_free(p)                pgrest_util_free_(p)

#if PGSQL_VERSION < 96
#define ALLOCSET_SMALL_MINSIZE    0
#define ALLOCSET_SMALL_INITSIZE  (1 * 1024)
#define ALLOCSET_SMALL_MAXSIZE   (8 * 1024)
#define ALLOCSET_SMALL_SIZES                        \
    ALLOCSET_SMALL_MINSIZE, ALLOCSET_SMALL_INITSIZE, ALLOCSET_SMALL_MAXSIZE
#endif

#define ALLOCSET_BUFFER_MINSIZE   (4 * 1024)
#define ALLOCSET_BUFFER_INITSIZE  (8 * 1024)
#define ALLOCSET_BUFFER_MAXSIZE   (8 * 1024)
#define ALLOCSET_BUFFER_SIZES                       \
    ALLOCSET_BUFFER_MINSIZE, ALLOCSET_BUFFER_INITSIZE, ALLOCSET_BUFFER_MAXSIZE


#if PGSQL_VERSION < 95
#if defined(__GNUC__) || defined(__IBMC__)
#define pg_attribute_printf(f,a) __attribute__((format(PG_PRINTF_ATTRIBUTE, f, a)))
#else
#define pg_attribute_printf(f,a)
#endif
#endif

#define PGREST_MEMBER_TO_STRUCT(s, m, p) ((s *)((char *)(p) - offsetof(s, m)))

void *pgrest_util_alloc_(MemoryContext mctx, Size size);
void *pgrest_util_calloc_(MemoryContext mctx, Size n, Size size);
void  pgrest_util_free_(void *pointer);

void  pgrest_util_sort(void *base, size_t n, size_t size,
                       int (*cmp)(const void *, const void *));
int   pgrest_util_atoi(const char *arg, size_t len);
char *pgrest_util_strlchr(char *p, char *last, char c);
MemoryContext pgrest_util_mctx_create(const char *name,
                                      Size min_size,
                                      Size init_size,
                                      Size max_size);
evutil_socket_t pgrest_util_mkstemp(const char *path);
int   pgrest_util_ncpu(void);

#endif /* PG_REST_UTIL_H */
