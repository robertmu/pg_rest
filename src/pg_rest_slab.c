/* -------------------------------------------------------------------------
 *
 * pq_rest_slab.c
 *   shared memory slab pool
 *   based on src/core/ngx_slab.c from NGINX copyright Igor Sysoev
 *
 * 
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_slab.c
 *
 * -------------------------------------------------------------------------
 */


#include "pg_rest_config.h"
#include "pg_rest_core.h"

#define PGREST_SLAB_PAGE_MASK   3
#define PGREST_SLAB_PAGE        0
#define PGREST_SLAB_BIG         1
#define PGREST_SLAB_EXACT       2
#define PGREST_SLAB_SMALL       3

#if (SIZEOF_VOID_P == 4)

#define PGREST_SLAB_PAGE_FREE   0
#define PGREST_SLAB_PAGE_BUSY   0xffffffff
#define PGREST_SLAB_PAGE_START  0x80000000

#define PGREST_SLAB_SHIFT_MASK  0x0000000f
#define PGREST_SLAB_MAP_MASK    0xffff0000
#define PGREST_SLAB_MAP_SHIFT   16

#define PGREST_SLAB_BUSY        0xffffffff

#else /* (SIZEOF_VOID_P == 8) */

#define PGREST_SLAB_PAGE_FREE   0
#define PGREST_SLAB_PAGE_BUSY   0xffffffffffffffff
#define PGREST_SLAB_PAGE_START  0x8000000000000000

#define PGREST_SLAB_SHIFT_MASK  0x000000000000000f
#define PGREST_SLAB_MAP_MASK    0xffffffff00000000
#define PGREST_SLAB_MAP_SHIFT   32

#define PGREST_SLAB_BUSY        0xffffffffffffffff

#endif


#if (PGREST_DEBUG)
#define pgrest_slab_junk(p, size)     MemSet(p, 0xA5, size)
#else
#define pgrest_slab_junk(p, size)
#endif

pgrest_uint_t  pgrest_pagesize;
pgrest_uint_t  pgrest_pagesize_shift;

static pgrest_uint_t  pgrest_slab_max_size;
static pgrest_uint_t  pgrest_slab_exact_size;
static pgrest_uint_t  pgrest_slab_exact_shift;

static pgrest_slab_page_t *pgrest_slab_alloc_pages(pgrest_slab_pool_t *pool,
                                                   pgrest_uint_t pages);
static void  pgrest_slab_free_pages(pgrest_slab_pool_t *pool, 
                                    pgrest_slab_page_t *page,
                                    pgrest_uint_t pages);
static void *pgrest_slab_do_alloc(pgrest_slab_pool_t *pool, size_t size);
static void *pgrest_slab_do_calloc(pgrest_slab_pool_t *pool, size_t size);
static void  pgrest_slab_do_free(pgrest_slab_pool_t *pool, void *p);

void
pgrest_slab_init(pgrest_slab_pool_t *pool)
{
    char                *p;
    size_t               size;
    pgrest_int_t         m;
    pgrest_uint_t        i, n, pages;
    pgrest_slab_page_t  *slots;

    /* STUB */
    if (pgrest_slab_max_size == 0) {
        pgrest_slab_max_size = pgrest_pagesize / 2;
        pgrest_slab_exact_size = pgrest_pagesize / (8 * sizeof(uintptr_t));
        for (n = pgrest_slab_exact_size; n >>= 1; pgrest_slab_exact_shift++) {
            /* void */
        }
    }
    /**/

    pool->min_size = 1 << pool->min_shift;

    p = (char *) pool + sizeof(pgrest_slab_pool_t);
    size = pool->end - p;

    pgrest_slab_junk(p, size);

    slots = (pgrest_slab_page_t *) p;
    n = pgrest_pagesize_shift - pool->min_shift;

    for (i = 0; i < n; i++) {
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(pgrest_slab_page_t);

    pages = (pgrest_uint_t) (size / (pgrest_pagesize + sizeof(pgrest_slab_page_t)));

    MemSet(p, 0, pages * sizeof(pgrest_slab_page_t));

    pool->pages = (pgrest_slab_page_t *) p;

    pool->free.prev = 0;
    pool->free.next = (pgrest_slab_page_t *) p;

    pool->pages->slab = pages;
    pool->pages->next = &pool->free;
    pool->pages->prev = (uintptr_t) &pool->free;

    pool->start = (char *)
                  PGREST_ALIGN_PTR((uintptr_t) p + pages * sizeof(pgrest_slab_page_t),
                                 pgrest_pagesize);

    m = pages - (pool->end - pool->start) / pgrest_pagesize;
    if (m > 0) {
        pages -= m;
        pool->pages->slab = pages;
    }

    pool->last = pool->pages + pages;
}


void *
pgrest_slab_alloc(pgrest_slab_pool_t *pool, size_t size)
{
    void  *p;

    LWLockAcquire(pool->lock, LW_EXCLUSIVE);

    p = pgrest_slab_do_alloc(pool, size);

    LWLockRelease(pool->lock);

    return p;
}


static void *
pgrest_slab_do_alloc(pgrest_slab_pool_t *pool, size_t size)
{
    size_t               s;
    uintptr_t            p, n, m, mask, *bitmap;
    pgrest_uint_t        i, slot, shift, map;
    pgrest_slab_page_t  *page, *prev, *slots;

    if (size > pgrest_slab_max_size) {

        ereport(DEBUG1, (errmsg(PGREST_PACKAGE " " "slab alloc: %zu", size)));

        page = pgrest_slab_alloc_pages(pool, (size >> pgrest_pagesize_shift)
                                          + ((size % pgrest_pagesize) ? 1 : 0));
        if (page) {
            p = (page - pool->pages) << pgrest_pagesize_shift;
            p += (uintptr_t) pool->start;

        } else {
            p = 0;
        }

        goto done;
    }

    if (size > pool->min_size) {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }
        slot = shift - pool->min_shift;

    } else {
        size = pool->min_size;
        shift = pool->min_shift;
        slot = 0;
    }

    slots = (pgrest_slab_page_t *) ((char *) pool + sizeof(pgrest_slab_pool_t));
    page = slots[slot].next;

    if (page->next != page) {

        if (shift < pgrest_slab_exact_shift) {

            do {
                p = (page - pool->pages) << pgrest_pagesize_shift;
                bitmap = (uintptr_t *) (pool->start + p);

                map = (1 << (pgrest_pagesize_shift - shift))
                          / (sizeof(uintptr_t) * 8);

                for (n = 0; n < map; n++) {

                    if (bitmap[n] != PGREST_SLAB_BUSY) {

                        for (m = 1, i = 0; m; m <<= 1, i++) {
                            if ((bitmap[n] & m)) {
                                continue;
                            }

                            bitmap[n] |= m;

                            i = ((n * sizeof(uintptr_t) * 8) << shift)
                                + (i << shift);

                            if (bitmap[n] == PGREST_SLAB_BUSY) {
                                for (n = n + 1; n < map; n++) {
                                    if (bitmap[n] != PGREST_SLAB_BUSY) {
                                        p = (uintptr_t) bitmap + i;

                                        goto done;
                                    }
                                }

                                prev = (pgrest_slab_page_t *)
                                            (page->prev & ~PGREST_SLAB_PAGE_MASK);
                                prev->next = page->next;
                                page->next->prev = page->prev;

                                page->next = NULL;
                                page->prev = PGREST_SLAB_SMALL;
                            }

                            p = (uintptr_t) bitmap + i;

                            goto done;
                        }
                    }
                }

                page = page->next;

            } while (page);

        } else if (shift == pgrest_slab_exact_shift) {

            do {
                if (page->slab != PGREST_SLAB_BUSY) {

                    for (m = 1, i = 0; m; m <<= 1, i++) {
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m;

                        if (page->slab == PGREST_SLAB_BUSY) {
                            prev = (pgrest_slab_page_t *)
                                            (page->prev & ~PGREST_SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = PGREST_SLAB_EXACT;
                        }

                        p = (page - pool->pages) << pgrest_pagesize_shift;
                        p += i << shift;
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);

        } else { /* shift > pgrest_slab_exact_shift */

            n = pgrest_pagesize_shift - (page->slab & PGREST_SLAB_SHIFT_MASK);
            n = 1 << n;
            n = ((uintptr_t) 1 << n) - 1;
            mask = n << PGREST_SLAB_MAP_SHIFT;

            do {
                if ((page->slab & PGREST_SLAB_MAP_MASK) != mask) {

                    for (m = (uintptr_t) 1 << PGREST_SLAB_MAP_SHIFT, i = 0;
                         m & mask;
                         m <<= 1, i++)
                    {
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m;

                        if ((page->slab & PGREST_SLAB_MAP_MASK) == mask) {
                            prev = (pgrest_slab_page_t *)
                                            (page->prev & ~PGREST_SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = PGREST_SLAB_BIG;
                        }

                        p = (page - pool->pages) << pgrest_pagesize_shift;
                        p += i << shift;
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);
        }
    }

    page = pgrest_slab_alloc_pages(pool, 1);

    if (page) {
        if (shift < pgrest_slab_exact_shift) {
            p = (page - pool->pages) << pgrest_pagesize_shift;
            bitmap = (uintptr_t *) (pool->start + p);

            s = 1 << shift;
            n = (1 << (pgrest_pagesize_shift - shift)) / 8 / s;

            if (n == 0) {
                n = 1;
            }

            bitmap[0] = (2 << n) - 1;

            map = (1 << (pgrest_pagesize_shift - shift)) / (sizeof(uintptr_t) * 8);

            for (i = 1; i < map; i++) {
                bitmap[i] = 0;
            }

            page->slab = shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | PGREST_SLAB_SMALL;

            slots[slot].next = page;

            p = ((page - pool->pages) << pgrest_pagesize_shift) + s * n;
            p += (uintptr_t) pool->start;

            goto done;

        } else if (shift == pgrest_slab_exact_shift) {

            page->slab = 1;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | PGREST_SLAB_EXACT;

            slots[slot].next = page;

            p = (page - pool->pages) << pgrest_pagesize_shift;
            p += (uintptr_t) pool->start;

            goto done;

        } else { /* shift > pgrest_slab_exact_shift */

            page->slab = ((uintptr_t) 1 << PGREST_SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | PGREST_SLAB_BIG;

            slots[slot].next = page;

            p = (page - pool->pages) << pgrest_pagesize_shift;
            p += (uintptr_t) pool->start;

            goto done;
        }
    }

    p = 0;

done:

    ereport(DEBUG1, (errmsg(PGREST_PACKAGE " " "slab alloc: %p", (void *)p)));

    return (void *) p;
}


void *
pgrest_slab_calloc(pgrest_slab_pool_t *pool, size_t size)
{
    void  *p;

    LWLockAcquire(pool->lock, LW_EXCLUSIVE);

    p = pgrest_slab_do_calloc(pool, size);

    LWLockRelease(pool->lock);

    return p;
}


static void *
pgrest_slab_do_calloc(pgrest_slab_pool_t *pool, size_t size)
{
    void  *p;

    p = pgrest_slab_do_alloc(pool, size);
    if (p) {
        MemSet(p, 0, size);
    }

    return p;
}


void
pgrest_slab_free(pgrest_slab_pool_t *pool, void *p)
{
    LWLockAcquire(pool->lock, LW_EXCLUSIVE);

    pgrest_slab_do_free(pool, p);

    LWLockRelease(pool->lock);
}


static void
pgrest_slab_do_free(pgrest_slab_pool_t *pool, void *p)
{
    size_t               size;
    uintptr_t            slab, m, *bitmap;
    pgrest_uint_t        n, type, slot, shift, map;
    pgrest_slab_page_t  *slots, *page;

    ereport(DEBUG1, (errmsg(PGREST_PACKAGE " " "slab free: %p", p)));

    if ((char *) p < pool->start || (char *) p > pool->end) {
        ereport(WARNING, (errmsg(PGREST_PACKAGE " " "pgrest_slab_do_free(): outside of pool")));
        goto fail;
    }

    n = ((char *) p - pool->start) >> pgrest_pagesize_shift;
    page = &pool->pages[n];
    slab = page->slab;
    type = page->prev & PGREST_SLAB_PAGE_MASK;

    switch (type) {

    case PGREST_SLAB_SMALL:

        shift = slab & PGREST_SLAB_SHIFT_MASK;
        size = 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        n = ((uintptr_t) p & (pgrest_pagesize - 1)) >> shift;
        m = (uintptr_t) 1 << (n & (sizeof(uintptr_t) * 8 - 1));
        n /= (sizeof(uintptr_t) * 8);
        bitmap = (uintptr_t *)
                             ((uintptr_t) p & ~((uintptr_t) pgrest_pagesize - 1));

        if (bitmap[n] & m) {

            if (page->next == NULL) {
                slots = (pgrest_slab_page_t *)
                                   ((char *) pool + sizeof(pgrest_slab_pool_t));
                slot = shift - pool->min_shift;

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | PGREST_SLAB_SMALL;
                page->next->prev = (uintptr_t) page | PGREST_SLAB_SMALL;
            }

            bitmap[n] &= ~m;

            n = (1 << (pgrest_pagesize_shift - shift)) / 8 / (1 << shift);

            if (n == 0) {
                n = 1;
            }

            if (bitmap[0] & ~(((uintptr_t) 1 << n) - 1)) {
                goto done;
            }

            map = (1 << (pgrest_pagesize_shift - shift)) / (sizeof(uintptr_t) * 8);

            for (n = 1; n < map; n++) {
                if (bitmap[n]) {
                    goto done;
                }
            }

            pgrest_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case PGREST_SLAB_EXACT:

        m = (uintptr_t) 1 <<
                (((uintptr_t) p & (pgrest_pagesize - 1)) >> pgrest_slab_exact_shift);
        size = pgrest_slab_exact_size;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        if (slab & m) {
            if (slab == PGREST_SLAB_BUSY) {
                slots = (pgrest_slab_page_t *)
                                   ((char *) pool + sizeof(pgrest_slab_pool_t));
                slot = pgrest_slab_exact_shift - pool->min_shift;

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | PGREST_SLAB_EXACT;
                page->next->prev = (uintptr_t) page | PGREST_SLAB_EXACT;
            }

            page->slab &= ~m;

            if (page->slab) {
                goto done;
            }

            pgrest_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case PGREST_SLAB_BIG:

        shift = slab & PGREST_SLAB_SHIFT_MASK;
        size = 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        m = (uintptr_t) 1 << ((((uintptr_t) p & (pgrest_pagesize - 1)) >> shift)
                              + PGREST_SLAB_MAP_SHIFT);

        if (slab & m) {

            if (page->next == NULL) {
                slots = (pgrest_slab_page_t *)
                                   ((char *) pool + sizeof(pgrest_slab_pool_t));
                slot = shift - pool->min_shift;

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | PGREST_SLAB_BIG;
                page->next->prev = (uintptr_t) page | PGREST_SLAB_BIG;
            }

            page->slab &= ~m;

            if (page->slab & PGREST_SLAB_MAP_MASK) {
                goto done;
            }

            pgrest_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case PGREST_SLAB_PAGE:

        if ((uintptr_t) p & (pgrest_pagesize - 1)) {
            goto wrong_chunk;
        }

        if (slab == PGREST_SLAB_PAGE_FREE) {
            ereport(WARNING, (errmsg(PGREST_PACKAGE " " "pgrest_slab_do_free(): page is already free")));
            goto fail;
        }

        if (slab == PGREST_SLAB_PAGE_BUSY) {
            ereport(WARNING, (errmsg(PGREST_PACKAGE " " "pgrest_slab_do_free(): pointer to wrong page")));
            goto fail;
        }

        n = ((char *) p - pool->start) >> pgrest_pagesize_shift;
        size = slab & ~PGREST_SLAB_PAGE_START;

        pgrest_slab_free_pages(pool, &pool->pages[n], size);

        pgrest_slab_junk(p, size << pgrest_pagesize_shift);

        return;
    }

    /* not reached */

    return;

done:

    pgrest_slab_junk(p, size);

    return;

wrong_chunk:

    ereport(WARNING, (errmsg(PGREST_PACKAGE " " "pgrest_slab_do_free(): pointer to wrong chunk")));

    goto fail;

chunk_already_free:

    ereport(WARNING, (errmsg(PGREST_PACKAGE " " "pgrest_slab_do_free(): chunk is already free")));

fail:

    return;
}


static pgrest_slab_page_t *
pgrest_slab_alloc_pages(pgrest_slab_pool_t *pool, pgrest_uint_t pages)
{
    pgrest_slab_page_t  *page, *p;

    for (page = pool->free.next; page != &pool->free; page = page->next) {

        if (page->slab >= pages) {

            if (page->slab > pages) {
                page[page->slab - 1].prev = (uintptr_t) &page[pages];

                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;
                page[pages].prev = page->prev;

                p = (pgrest_slab_page_t *) page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t) &page[pages];

            } else {
                p = (pgrest_slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | PGREST_SLAB_PAGE_START;
            page->next = NULL;
            page->prev = PGREST_SLAB_PAGE;

            if (--pages == 0) {
                return page;
            }

            for (p = page + 1; pages; pages--) {
                p->slab = PGREST_SLAB_PAGE_BUSY;
                p->next = NULL;
                p->prev = PGREST_SLAB_PAGE;
                p++;
            }

            return page;
        }
    }

    ereport(WARNING, (errmsg(PGREST_PACKAGE " " "pgrest_slab_alloc_pages() failed: no memory")));

    return NULL;
}


static void
pgrest_slab_free_pages(pgrest_slab_pool_t *pool, pgrest_slab_page_t *page,
    pgrest_uint_t pages)
{
    pgrest_uint_t        type;
    pgrest_slab_page_t  *prev, *join;

    page->slab = pages--;

    if (pages) {
        MemSet(&page[1], 0, pages * sizeof(pgrest_slab_page_t));
    }

    if (page->next) {
        prev = (pgrest_slab_page_t *) (page->prev & ~PGREST_SLAB_PAGE_MASK);
        prev->next = page->next;
        page->next->prev = page->prev;
    }

    join = page + page->slab;

    if (join < pool->last) {
        type = join->prev & PGREST_SLAB_PAGE_MASK;

        if (type == PGREST_SLAB_PAGE) {

            if (join->next != NULL) {
                pages += join->slab;
                page->slab += join->slab;

                prev = (pgrest_slab_page_t *) (join->prev & ~PGREST_SLAB_PAGE_MASK);
                prev->next = join->next;
                join->next->prev = join->prev;

                join->slab = PGREST_SLAB_PAGE_FREE;
                join->next = NULL;
                join->prev = PGREST_SLAB_PAGE;
            }
        }
    }

    if (page > pool->pages) {
        join = page - 1;
        type = join->prev & PGREST_SLAB_PAGE_MASK;

        if (type == PGREST_SLAB_PAGE) {

            if (join->slab == PGREST_SLAB_PAGE_FREE) {
                join = (pgrest_slab_page_t *) (join->prev & ~PGREST_SLAB_PAGE_MASK);
            }

            if (join->next != NULL) {
                pages += join->slab;
                join->slab += page->slab;

                prev = (pgrest_slab_page_t *) (join->prev & ~PGREST_SLAB_PAGE_MASK);
                prev->next = join->next;
                join->next->prev = join->prev;

                page->slab = PGREST_SLAB_PAGE_FREE;
                page->next = NULL;
                page->prev = PGREST_SLAB_PAGE;

                page = join;
            }
        }
    }

    if (pages) {
        page[pages].prev = (uintptr_t) page;
    }

    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}
