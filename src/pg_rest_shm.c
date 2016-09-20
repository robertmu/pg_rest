/* -------------------------------------------------------------------------
 *
 * pq_rest_shm.c
 *   shared memory routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_shm.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

static shmem_startup_hook_type   pgrest_shm_pre_hook = NULL;
static List                     *pgrest_shm_segments = NIL;
#if PGSQL_VERSION >= 96
static int                       pgrest_shm_lock_index = 0;
#endif 

static void pgrest_shm_shutdown(int code, Datum arg);

static size_t
pgrest_shm_mem_size(void)
{
    ListCell           *cell;
    pgrest_shm_seg_t   *seg;
    size_t              result = 0;

    /* for each shared memory segment */
    foreach(cell, pgrest_shm_segments) {
        seg = lfirst(cell);
        result += seg->size;
    }

    return result;
}

#if PGSQL_VERSION >= 96
LWLock *
#else
LWLockId
#endif
pgrest_shm_get_lock(void)
{
#if PGSQL_VERSION >= 96
    return &((GetNamedLWLockTranche(PGREST_PACKAGE))
                                [pgrest_shm_lock_index++].lock);
#else
    return LWLockAssign();
#endif
}


static int 
pgrest_shm_lock_count(void)
{
    ListCell           *cell;
    pgrest_shm_seg_t   *seg;
    int                 result = 0;

    /* for each shared memory segment */
    foreach(cell, pgrest_shm_segments) {
        seg = lfirst(cell);
        if (seg->lock) {
            result++;
        }
    }

    return result;
}

pgrest_shm_seg_t *
pgrest_shm_seg_add(char *name, size_t size, bool lock, bool pool)
{
    ListCell           *cell;
    pgrest_shm_seg_t   *seg;

    /* for each shared memory segment */
    foreach(cell, pgrest_shm_segments) {
        seg = lfirst(cell);

        if (strcmp(name, seg->name) != 0) {
            continue;
        }

        if (seg->size == 0) {
            seg->size = size;
        }

        if (size && size != seg->size) {
            ereport(WARNING,
                    (errmsg(PGREST_PACKAGE " " "the size %zu of shared memory segment "
                            "\"%s\" conflicts with already declared size %zu",
                             size, seg->name, seg->size)));
            return NULL;
        }

        return seg;
    }

    seg = (pgrest_shm_seg_t *) palloc0fast(sizeof(pgrest_shm_seg_t));

    seg->size = size;
    seg->name = pstrdup(name);
    seg->init = NULL;
    seg->lock = lock;
    seg->pool = pool;

    pgrest_shm_segments = lappend(pgrest_shm_segments, seg);

    return seg;
}

static void
pgrest_shm_startup(void)
{
    pgrest_shm_seg_t   *seg;
    ListCell           *cell;
    bool                found;
    void               *addr;
    pgrest_slab_pool_t *pool;

    if (pgrest_shm_pre_hook) {
        pgrest_shm_pre_hook();
    }

    /* for each shared memory segment */
    foreach(cell, pgrest_shm_segments) {
        seg = lfirst(cell);

        LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

        addr = ShmemInitStruct(seg->name, seg->size, &found);

        LWLockRelease(AddinShmemInitLock);

        Assert(!found);

        seg->addr = addr;
        if (seg->pool) {
            pool = (pgrest_slab_pool_t *) seg->addr;
            pool->end = seg->addr + seg->size;
            pool->min_shift = 3;
            pool->addr = seg->addr;

            if (seg->lock) {
#if PGSQL_VERSION >= 96
                pool->lock = &((GetNamedLWLockTranche(PGREST_PACKAGE))
                                [pgrest_shm_lock_index++].lock);
#else
                pool->lock = LWLockAssign();
#endif
            }

            pgrest_slab_init(pool);
        }

        if (seg->init) {
            if (seg->init(seg) == false) {
                ereport(ERROR,
                        (errmsg(PGREST_PACKAGE " " "initialize shared memory segment failed")));
            }
        }
    }

    /*
     * If we're in the postmaster (or a standalone backend...), set up a
     * shmem exit hook to do other work
     */
    if (!IsUnderPostmaster)
        on_shmem_exit(pgrest_shm_shutdown, (Datum) 0);

#if (PGREST_DEBUG)
    pgrest_shm_info_print();
#endif
}

static void
pgrest_shm_shutdown(int code, Datum arg)
{
    /* Don't try to dump during a crash. */
    if (code) {
        return;
    }

    /* Other stuff */
}

void
pgrest_shm_fini(void)
{
    /* Uninstall hooks. */
    shmem_startup_hook = pgrest_shm_pre_hook;
    debug_log(DEBUG1, (errmsg(PGREST_PACKAGE " " "shm fini")));
}

static StringInfo
pgrest_shm_info_format(List *shm_segments)
{
    StringInfo             result;
    ListCell              *cell;
    pgrest_shm_seg_t      *seg;

    result = makeStringInfo();

    /* for each listening socket */
    foreach(cell, shm_segments) {
        seg = lfirst(cell);

        appendStringInfo(result,"\n");

        appendStringInfo(result,
            "!\tsegment addr:%p\n", seg->addr);
        appendStringInfo(result,
            "!\tsegment name %s\n", seg->name);
        appendStringInfo(result,
            "!\tsegment size %zu\n", seg->size);
        appendStringInfo(result,
            "!\tsegment init %p\n", seg->init);
        appendStringInfo(result,
            "!\tsegment lock %d\n", seg->lock);
        appendStringInfo(result,
            "!\tsegment pool %d\n", seg->pool);
    }

    return result;
}

void
pgrest_shm_info_print(void)
{
    StringInfo buffer;

    buffer = pgrest_shm_info_format(pgrest_shm_segments);

    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print %s shm segment ++++++++++", 
                            !IsUnderPostmaster ? "postmaster" : "worker")));
    ereport(DEBUG1,(errmsg("\n%s", buffer->data)));
    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print %s shm segment ----------", 
                            !IsUnderPostmaster ? "postmaster" : "worker")));

    pfree(buffer->data);
    pfree(buffer);
}

void
pgrest_shm_init(void)
{
    RequestAddinShmemSpace(pgrest_shm_mem_size());
#if PGSQL_VERSION >= 96
    RequestNamedLWLockTranche(PGREST_PACKAGE, pgrest_shm_lock_count());
#else
    RequestAddinLWLocks(pgrest_shm_lock_count());
#endif
    /* Install hooks */
    pgrest_shm_pre_hook = shmem_startup_hook;
    shmem_startup_hook = pgrest_shm_startup;
}
