/*-------------------------------------------------------------------------
 *
 * include/pg_rest_slab.h
 *
 * shared memory slab pool
 * based on src/core/ngx_slab.c from NGINX copyright Igor Sysoev
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_SLAB_H
#define PG_REST_SLAB_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct pgrest_slab_page_s  pgrest_slab_page_t;

struct pgrest_slab_page_s {
    uintptr_t            slab;
    pgrest_slab_page_t  *next;
    uintptr_t            prev;
};

typedef struct {
#if PGSQL_VERSION >= 96
    LWLock              *lock;
#else
    LWLockId             lock;
#endif
    size_t               min_size;
    size_t               min_shift;

    pgrest_slab_page_t  *pages;
    pgrest_slab_page_t  *last;
    pgrest_slab_page_t   free;

    char                *start;
    char                *end;

    void                *data;
    void                *addr;
} pgrest_slab_pool_t;

extern pgrest_uint_t  pgrest_pagesize;
extern pgrest_uint_t  pgrest_pagesize_shift;

void  pgrest_slab_init(pgrest_slab_pool_t *pool);
void *pgrest_slab_alloc(pgrest_slab_pool_t *pool, size_t size);
void *pgrest_slab_calloc(pgrest_slab_pool_t *pool, size_t size);
void  pgrest_slab_free(pgrest_slab_pool_t *pool, void *p);

#endif /* PG_REST_SLAB_H */
