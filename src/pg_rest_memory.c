/* -------------------------------------------------------------------------
 *
 * pq_rest_memory.c
 *   memory & buffer mgmt routines 
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_memory.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

pgrest_mpool_t *
pgrest_mpool_create(void)
{
    pgrest_mpool_t *pool = NULL;
    MemoryContext   mctx = NULL;

    mctx = pgrest_util_mctx_create(PGREST_PACKAGE " " "mpool",
                                   ALLOCSET_BUFFER_SIZES);
    if (mctx == NULL) {
        return NULL;
    }

    pool = pgrest_util_alloc(mctx, sizeof(pgrest_mpool_t));

    Assert(pool != NULL);

    pool->mctx = mctx;
    pool->chain = NULL;

    return pool;
}

pgrest_buffer_t *
pgrest_buffer_create(pgrest_mpool_t *pool, size_t size)
{
    pgrest_buffer_t *b;

    b = pgrest_mpool_alloc(pool, offsetof(pgrest_buffer_t, start) + size);
    if (b == NULL) {
        return NULL;
    }

    b->capacity = size;
    b->size = 0;
    b->pos = b->start;
    b->fd = (evutil_socket_t) -1;

    return b;
}

pgrest_chain_t *
pgrest_chain_link_alloc(pgrest_mpool_t *pool)
{
    pgrest_chain_t  *cl;

    cl = pool->chain;

    if (cl) {
        pool->chain = cl->next;
        return cl;
    }

    cl = pgrest_mpool_alloc(pool, sizeof(pgrest_chain_t));
    if (cl == NULL) {
        return NULL;
    }

    return cl;
}

bool
pgrest_chain_append(pgrest_mpool_t *pool, 
                    pgrest_chain_t **chain, 
                    pgrest_chain_t *in)
{
    pgrest_chain_t  *cl, **ll;

    ll = chain;

    for (cl = *chain; cl; cl = cl->next) {
        ll = &cl->next;
    }

    while (in) {
        cl = pgrest_chain_link_alloc(pool);
        if (cl == NULL) {
            return false;
        }

        cl->buf = in->buf;
        *ll = cl;
        ll = &cl->next;
        in = in->next;
    }

    *ll = NULL;

    return true;
}

pgrest_chain_t *
pgrest_chain_update_sent(pgrest_chain_t *in, size_t sent)
{
    size_t  size;

    for ( /* void */ ; in; in = in->next) {

        if (sent == 0) {
            break;
        }

        size = in->buf->size;

        if (sent >= size) {
            sent -= size;
            in->buf->pos = in->buf->start;
            in->buf->size = 0;

            continue;
        }

        in->buf->pos += sent;
        in->buf->size -= sent;

        break;
    }

    return in;
}

static inline size_t topagesize(size_t capacity)
{
    size_t pagesize = pgrest_pagesize;
    return (offsetof(pgrest_buffer_t, start) + capacity + pagesize - 1)
               / pagesize * pagesize;
}

pgrest_iovec_t 
pgrest_buffer_reserve(pgrest_mpool_t   *pool, 
                      pgrest_buffer_t **inbuf, 
                      size_t            size)
{
    pgrest_iovec_t      result;
    size_t              capacity, alloc_size;
    pgrest_buffer_t    *newb;
    evutil_socket_t     fd  = (evutil_socket_t) -1;
    pgrest_buffer_t    *buf = *inbuf;

    capacity = buf->capacity;

    if (buf->capacity - buf->size - (buf->pos - buf->start) >= size) {
        goto done;
    }

    if ((buf->size + size) * 2 <= buf->capacity) {
        memmove(buf->start, buf->pos, buf->size);
        buf->pos = buf->start;
        goto done;
    }

    do {
        capacity <<= 1;
    } while (capacity - buf->size < size);

    if (capacity <= PGREST_BUFFER_LIMIT_SIZE) {
        newb = pgrest_mpool_alloc(pool, offsetof(pgrest_buffer_t, start) + 
                                        capacity);
        goto copy;
    }

    alloc_size = topagesize(capacity);
    fd = buf->fd;

    if (fd == (evutil_socket_t) -1 &&
        (fd = pgrest_util_mkstemp(pgrest_setting.temp_buffer_path)) == -1) {
        goto fail;
    }

    if (posix_fallocate(fd, 0, alloc_size) != 0) {
        /* TODO log */
        goto fail;
    }

    if ((newb = (void *)mmap(NULL, alloc_size, PROT_READ|PROT_WRITE, 
                              MAP_SHARED, fd, 0 )) == MAP_FAILED) {
        /* TODO log */
        goto fail;
    }

    if (buf->fd != (evutil_socket_t) -1) {
        size_t pos = buf->pos - buf->start;

        munmap((void *)buf, topagesize(buf->capacity));
        *inbuf = buf = newb;
        buf->capacity = capacity;
        buf->pos = newb->start + pos;
        goto done;
    }

copy:
    memcpy(newb->start, buf->pos, buf->size);
    pgrest_buffer_init(newb, capacity, buf->size, newb->start, fd); 
    pgrest_buffer_free(buf);
    *inbuf = buf = newb;

done:
    result.base = buf->pos + buf->size;
    result.len = buf->start + buf->capacity - result.base;
    return result;

fail:
    result.base = NULL;
    result.len = 0;
    return result;
}

void 
pgrest_buffer_free(pgrest_buffer_t *buffer)
{
    if (buffer->fd != (evutil_socket_t) -1) {
        close(buffer->fd);
        munmap((void *)buffer, topagesize(buffer->capacity));
    } else {
        pgrest_mpool_free(buffer);
    }
}

void 
pgrest_buffer_consume(pgrest_buffer_t *inbuf, size_t size)
{
    if (inbuf->size == size) {
        inbuf->size = 0;
        inbuf->pos = inbuf->start;
    } else {
        inbuf->size -= size;
        inbuf->pos += size;
    }
}
