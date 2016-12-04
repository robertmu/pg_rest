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
    MemoryContext   mctx;
    size_t          elt_size;
    pgrest_uint_t   size;
    pgrest_uint_t   capacity;
} pgrest_array_t;

static inline bool
pgrest_array_init(pgrest_array_t *array,
                  MemoryContext mctx,
                  pgrest_uint_t capacity,
                  size_t        elt_size)
{
    array->size = 0;
    array->mctx = mctx;
    array->capacity = capacity;
    array->elt_size = elt_size;

    array->elts = pgrest_util_alloc(mctx, capacity * elt_size);

    if (array->elts == NULL) {
        return false;
    }

    return true;
}

pgrest_array_t *pgrest_array_create(MemoryContext mctx, 
                                    pgrest_uint_t capacity, 
                                    size_t elt_size);
void  pgrest_array_destroy(pgrest_array_t *array);
void *pgrest_array_push(pgrest_array_t *array);
void *pgrest_array_push_head(pgrest_array_t *array);
bool  pgrest_array_copy(pgrest_array_t *array_dst, pgrest_array_t *array_src);

#endif /* PG_REST_ARRAY_H */
