/*-------------------------------------------------------------------------
 *
 * include/pg_rest_array.h
 *
 * dynamic array routines 
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_ARRAY_H
#define PG_REST_ARRAY_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct {
    void           *elts;
    size_t          elt_size;
    pgrest_uint_t   size;
    pgrest_uint_t   capacity;
} pgrest_array_t;

#define pgrest_array_init(array, n, s)    \
do {                                      \
    (array)->size = 0;                    \
    (array)->capacity = (n);              \
    (array)->elt_size = (s);              \
    (array)->elts = palloc((n) * (s));    \
} while (0)

pgrest_array_t *pgrest_array_create(pgrest_uint_t capacity, size_t elt_size);
void pgrest_array_destroy(pgrest_array_t *array);
void *pgrest_array_push(pgrest_array_t *array);
void pgrest_array_copy(pgrest_array_t *array_dst, pgrest_array_t *array_src);

#endif /* PG_REST_ARRAY_H */
