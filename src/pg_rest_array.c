/* -------------------------------------------------------------------------
 *
 * pq_rest_array.c
 *   dynamic array routines 
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_array.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

pgrest_array_t *
pgrest_array_create(MemoryContext mctx, pgrest_uint_t capacity, size_t elt_size)
{
    pgrest_array_t *array;

    array = pgrest_util_alloc(mctx, sizeof(pgrest_array_t));
    if (array == NULL) {
        return NULL;
    }

    if (pgrest_array_init(array, mctx, capacity, elt_size) == false ) {
        return NULL;
    }

    return array;
}

void
pgrest_array_destroy(pgrest_array_t *array)
{
    pgrest_util_free(array->elts);
    pgrest_util_free(array);
}

void *
pgrest_array_push(pgrest_array_t *array)
{
    void    *elt = NULL;
    size_t   size;
    void    *p;

    if (array->size == array->capacity) {
        size = array->elt_size * array->capacity;

        p = pgrest_util_alloc(array->mctx, 2 * size);
        if (p == NULL) {
            return NULL;
        }

        memcpy(p, array->elts, size);
        array->elts = p;
        array->capacity *= 2;
    }

    elt = (char *) array->elts + array->elt_size * array->size;
    array->size++;

    return elt;
}

void *
pgrest_array_push_head(pgrest_array_t *array)
{
    void *elt;

    elt = pgrest_array_push(array);
    if (array->size == 1) {
        return elt;
    }

    memmove((char *)array->elts + array->elt_size,
            array->elts,
            (array->size - 1) * array->elt_size);

    return array->elts;
}

bool
pgrest_array_copy(pgrest_array_t *array_dst, pgrest_array_t *array_src)
{
    if (array_dst->elts) {
        pgrest_util_free(array_dst->elts);
    }

    array_dst->elts = pgrest_util_alloc(array_dst->mctx, 
                                        array_src->capacity * 
                                          array_src->elt_size);
    if (array_dst->elts == NULL) {
        return false;
    }

    memcpy(array_dst->elts, array_src->elts, 
           array_src->elt_size * array_src->size);

    array_dst->elt_size = array_src->elt_size;
    array_dst->size = array_src->size;
    array_dst->capacity = array_src->capacity;

    return true;
}
