/*-------------------------------------------------------------------------
 *
 * include/pg_rest_shm.h
 *
 * shared memory mgmt.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_SHM_H
#define PG_REST_SHM_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct pgrest_shm_seg_s  pgrest_shm_seg_t;

typedef bool (*pgrest_shm_seg_init_pt) (pgrest_shm_seg_t *seg);

struct pgrest_shm_seg_s {
    char                     *addr;
    char                     *name;
    size_t                    size;
    pgrest_shm_seg_init_pt    init;
    unsigned                  lock:1;
    unsigned                  pool:1;
};

pgrest_shm_seg_t *
pgrest_shm_seg_add(char *name, size_t size, bool lock, bool pool);
void pgrest_shm_init(void);
void pgrest_shm_fini(void);
void pgrest_shm_info_print(void);
#if PGSQL_VERSION >= 96
LWLock *pgrest_shm_get_lock(void);
#else
LWLockId pgrest_shm_get_lock(void);
#endif

#endif /* PG_REST_SHM_H */
