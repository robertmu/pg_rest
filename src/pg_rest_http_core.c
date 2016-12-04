/* -------------------------------------------------------------------------
 *
 * pq_rest_http_core.c
 *   http path filter/handler routines.
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http_core.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

pgrest_http_handler_t *
pgrest_http_handler_create(pgrest_conf_http_path_t *path, 
                           size_t size)
{
    pgrest_http_handler_t  *handler;
    pgrest_http_handler_t **save;
    
    if (path->handlers.elts == NULL) {
        if (pgrest_array_init(&path->handlers, 
                              CurrentMemoryContext, 
                              2, 
                              sizeof(pgrest_http_handler_t *)) == false)
        {
            return NULL;
        }
    }

    handler = palloc0(size);

    save = pgrest_array_push(&path->handlers);
    if (save == NULL) {
        return NULL;
    }

   *save = handler;

    return handler;
}

pgrest_http_filter_t *
pgrest_http_filter_create(pgrest_conf_http_path_t *path, 
                          size_t size)
{
    pgrest_http_filter_t  *filter;
    pgrest_http_filter_t **save;

    if (path->filters.elts == NULL) {
        if (pgrest_array_init(&path->filters, 
                              CurrentMemoryContext, 
                              2, 
                              sizeof(pgrest_http_filter_t *)) == false) 
        {
            return NULL;
        }
    }
    
    filter = palloc0(size);
    
    save = pgrest_array_push_head(&path->filters);
    if (save == NULL) {
        return NULL;
    }

   *save = filter;

    return filter;
}

void 
pgrest_http_process_request(pgrest_http_request_t *req)
{

}

