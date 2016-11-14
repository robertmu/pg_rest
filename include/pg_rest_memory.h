/*-------------------------------------------------------------------------
 *
 * include/pg_rest_memory.h
 *
 * memory & bufer mgmt routines 
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_MEMORY_H
#define PG_REST_MEMORY_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

#define PGREST_BUFFER_LIMIT_SIZE 1048576                   /* 1024 * 1024 */
#define PGREST_BUFFER_DTEMP_PATH "/tmp/pg_rest.b.XXXXXX"

typedef struct pgrest_buffer_s   pgrest_buffer_t;
typedef struct pgrest_chain_s    pgrest_chain_t;

typedef struct {
    MemoryContext                mctx;
    pgrest_chain_t              *chain;
} pgrest_mpool_t;

struct pgrest_buffer_s {
    size_t                       capacity;
    size_t                       size;
    unsigned char               *pos;
    evutil_socket_t              fd;
    unsigned char                start[FLEXIBLE_ARRAY_MEMBER];
};

struct pgrest_chain_s {
    pgrest_buffer_t             *buf;
    pgrest_chain_t              *next;
};

static inline pgrest_iovec_t 
pgrest_iovec_init(const void *base, size_t len)
{
    pgrest_iovec_t vec;

    vec.base = (unsigned char *)base;
    vec.len = len;

    return vec;
}

static inline void 
pgrest_mpool_reset(pgrest_mpool_t *pool)
{
    pool->chain = NULL;
    MemoryContextReset(pool->mctx);
}

static inline void 
pgrest_mpool_destroy(pgrest_mpool_t *pool)
{
    MemoryContextDelete(pool->mctx);
}

static inline void *
pgrest_mpool_alloc(pgrest_mpool_t *pool, size_t size)
{
    return pgrest_util_alloc(pool->mctx, size);
}

static inline void *
pgrest_mpool_calloc(pgrest_mpool_t *pool, size_t n, size_t size)
{
    return pgrest_util_calloc(pool->mctx, n, size);
}

static inline void
pgrest_mpool_free(void *p)
{
    pgrest_util_free(p);
}

pgrest_mpool_t *pgrest_mpool_create(void);

#define pgrest_buffer_init(b, c, s, p, f)                                   \
do {                                                                        \
    (b)->capacity = (c);                                                    \
    (b)->size = (s);                                                        \
    (b)->pos = (p);                                                         \
    (b)->fd = (f);                                                          \
} while (0)

#define pgrest_free_chain(pool, cl)                                         \
    cl->next = pool->chain;                                                 \
    pool->chain = cl

pgrest_buffer_t *pgrest_buffer_create(pgrest_mpool_t *pool, size_t size);
pgrest_chain_t *pgrest_chain_link_alloc(pgrest_mpool_t *pool);
bool pgrest_chain_append(pgrest_mpool_t *pool, 
                         pgrest_chain_t **chain, 
                         pgrest_chain_t *in);
pgrest_chain_t *pgrest_chain_update_sent(pgrest_chain_t *in, size_t sent);
pgrest_iovec_t pgrest_buffer_reserve(pgrest_mpool_t *pool, 
                         pgrest_buffer_t **inbuf, 
                         size_t size);
void pgrest_buffer_consume(pgrest_buffer_t *inbuf, size_t size);
void pgrest_buffer_free(pgrest_buffer_t *buffer);

#endif /* PG_REST_MEMORY_H */
