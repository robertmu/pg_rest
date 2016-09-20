/*-------------------------------------------------------------------------
 *
 * include/pg_rest_cqueue.h
 *
 * circular queue routines 
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_CQUEUE_H
#define PG_REST_CQUEUE_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct {
    void           *elts;
    size_t          elt_size;
    pgrest_uint_t   size;
    pgrest_uint_t   capacity;
    pgrest_uint_t   head;
    pgrest_uint_t   tail;
} pgrest_cqueue_t;

pgrest_cqueue_t *pgrest_cqueue_create(pgrest_uint_t capacity, size_t elt_size);
void pgrest_cqueue_destroy(pgrest_cqueue_t *cq);
void *pgrest_cqueue_push(pgrest_cqueue_t *cq);
void *pgrest_cqueue_pop(pgrest_cqueue_t *cq);

#endif /* PG_REST_CQUEUE_H */
