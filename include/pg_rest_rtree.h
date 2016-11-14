/*-------------------------------------------------------------------------
 *
 * include/pg_rest_rtree.h
 *
 * 
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_RTREE_H
#define PG_REST_RTREE_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct pgrest_rtree_node_s pgrest_rtree_node_t;

pgrest_rtree_node_t *pgrest_rtree_create(void);
void pgrest_rtree_destroy(pgrest_rtree_node_t *root);
bool pgrest_rtree_add(pgrest_rtree_node_t *node, 
                      pgrest_string_t      path, 
                      void                *handler);
bool pgrest_rtree_find(pgrest_rtree_node_t  *node, 
                      pgrest_string_t        path,
                      void                 **handler,
                      pgrest_param_t        *params, 
                      size_t                *num_params);
void pgrest_rtree_walker(pgrest_rtree_node_t *root, void (*walker) ());

#endif /* PG_REST_RTREE_H_ */
