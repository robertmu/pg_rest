/* -------------------------------------------------------------------------
 *
 * pq_rest_cqueue.c
 *   circular queue routines 
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_cqueue.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

pgrest_cqueue_t *
pgrest_cqueue_create(pgrest_uint_t capacity, size_t elt_size)
{
    pgrest_cqueue_t *cq;

    cq = palloc(sizeof(pgrest_cqueue_t));

    cq->size = 0;
    cq->elt_size = elt_size;
    cq->capacity = capacity;
    cq->head = 0;
    cq->tail = 0;

    cq->elts = palloc(capacity * elt_size);

    return cq;
}

void
pgrest_cqueue_destroy(pgrest_cqueue_t *cq)
{
    pfree(cq->elts);
    pfree(cq);
}

void *
pgrest_cqueue_push(pgrest_cqueue_t *cq)
{
    void    *elt = NULL;

    if (cq->size == cq->capacity) {
        return elt;
    }

    elt = (char *) cq->elts + cq->elt_size * cq->tail;
    cq->tail = (cq->tail + 1) % cq->capacity;
    cq->size++;

    return elt;
}

void *
pgrest_cqueue_pop(pgrest_cqueue_t *cq)
{
    void    *elt = NULL;

    if (cq->size == 0) {
        return elt;
    }

    elt = (char *) cq->elts + cq->elt_size * cq->head;
    cq->head = (cq->head + 1) % cq->capacity;
    cq->size--;

    return elt;
}
