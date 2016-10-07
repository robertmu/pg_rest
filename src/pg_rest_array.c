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
pgrest_array_create(pgrest_uint_t capacity, size_t elt_size)
{
    pgrest_array_t *array;

    array = palloc(sizeof(pgrest_array_t));
    pgrest_array_init(array, capacity, elt_size);

    return array;
}

void
pgrest_array_destroy(pgrest_array_t *array)
{
    pfree(array->elts);
    pfree(array);
}

void *
pgrest_array_push(pgrest_array_t *array)
{
    void    *elt = NULL;
    size_t   size;

    if (array->size == array->capacity) {
        size = array->elt_size * array->capacity;

        array->elts = repalloc(array->elts, 2 * size);
        array->capacity *= 2;
    }

    elt = (char *) array->elts + array->elt_size * array->size;
    array->size++;

    return elt;
}

void
pgrest_array_copy(pgrest_array_t *array_dst, pgrest_array_t *array_src)
{
    if (array_dst->elts) {
        pfree(array_dst->elts);
    }

    array_dst->elts = palloc(array_src->capacity * array_src->elt_size);
    memcpy(array_dst->elts, array_src->elts, 
           array_src->elt_size * array_src->size);

    array_dst->elt_size = array_src->elt_size;
    array_dst->size = array_src->size;
    array_dst->capacity = array_src->capacity;
}
