/*-------------------------------------------------------------------------
 *
 * include/pg_rest_config.h
 *
 * 
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_CONFIG_H
#define PG_REST_CONFIG_H

#include <postgres.h>
#include "pg_rest_header.h"

#define PGREST_ALIGN_PTR(LEN,ALIGNVAL)       TYPEALIGN(ALIGNVAL, (LEN))
#if PGSQL_VERSION < 95
#define PG_CACHE_LINE_SIZE                   128
#define CACHELINEALIGN(LEN)                  TYPEALIGN(PG_CACHE_LINE_SIZE, (LEN))
#endif

typedef uintptr_t                            pgrest_uint_t;
typedef intptr_t                             pgrest_int_t;

#define PGREST_PACKAGE                       "pg_rest"

#endif /* PG_REST_CONFIG_H_ */
