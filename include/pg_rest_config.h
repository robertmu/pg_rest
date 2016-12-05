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

#define PGREST_ALIGN_PTR(LEN,ALIGNVAL)   TYPEALIGN(ALIGNVAL, (LEN))
#if PGSQL_VERSION < 95
#define PG_CACHE_LINE_SIZE               128
#define CACHELINEALIGN(LEN)              TYPEALIGN(PG_CACHE_LINE_SIZE, (LEN))
#define PG_INT32_MAX                     2147483647
#define PG_INT64_MAX                     9223372036854775807LL
#endif

typedef uintptr_t                        pgrest_uint_t;
typedef intptr_t                         pgrest_int_t;

#define PGREST_PACKAGE                   "pg_rest"
#define PGREST_AF_SIZE                   16

#if (SIZEOF_VOID_P == 4)
#define PGREST_MAX_TIME_T_VALUE          PG_INT32_MAX
#define PGREST_MAX_OFF_T_VALUE           PG_INT64_MAX 
#else /* (SIZEOF_VOID_P == 8) */
#define PGREST_MAX_TIME_T_VALUE          PG_INT64_MAX
#define PGREST_MAX_OFF_T_VALUE           PG_INT64_MAX 
#endif

#endif /* PG_REST_CONFIG_H */
