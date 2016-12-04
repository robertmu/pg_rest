/* -------------------------------------------------------------------------
 *
 * pq_rest_util.c
 *   misc routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_util.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

int pgrest_tcp_nodelay_and_nopush;

void
pgrest_os_init(void)
{
    pgrest_uint_t  i;

    pgrest_pagesize = getpagesize();
    for (i = pgrest_pagesize; i >>= 1; pgrest_pagesize_shift++) { /* void */ }
}

MemoryContext
pgrest_util_mctx_create(MemoryContext parent,
                        const char *name,
                        Size min_size,
                        Size init_size,
                        Size max_size)
{
    MemoryContext mctx = NULL;
    MemoryContext cctx = CurrentMemoryContext;

    PG_TRY();
    {
        mctx = AllocSetContextCreate(parent, 
                                     name, 
                                     min_size,
                                     init_size,
                                     max_size);
    }
    PG_CATCH();
    {
        EmitErrorReport();
        FlushErrorState();
        /* we are in errorcontext,so we need to restore memorycontext */
        MemoryContextSwitchTo(cctx);
    }
    PG_END_TRY();

    return mctx;
}

/*
 * pgrest_util_alloc_
 *    Allocate space within the specified context.
 *
 */
void *
pgrest_util_alloc_(MemoryContext mctx, Size size)
{
    AssertArg(MemoryContextIsValid(mctx));

    if (!AllocSizeIsValid(size)) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "invalid memory "
                              "alloc request size %zu", size)));
        return NULL;
    }

    mctx->isReset = false;

    /* TODO log iff failed */

    return (*mctx->methods->alloc) (mctx, size);
}

void *
pgrest_util_calloc_(MemoryContext mctx, Size n, Size size)
{
    void    *ret;
    Size     total = n * size;  

    ret = pgrest_util_alloc_(mctx, total);
    if (ret == NULL) {
        return NULL;
    }

    MemSetAligned(ret, 0, total);

    return ret;
}

void *
pgrest_util_realloc_(void *pointer, Size size)
{
    MemoryContext context;

    if (!AllocSizeIsValid(size)) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "invalid memory "
                              "alloc request size %zu", size)));
        return NULL;
    }

    /*
     * Try to detect bogus pointers handed to us, poorly though we can.
     * Presumably, a pointer that isn't MAXALIGNED isn't pointing at an
     * allocated chunk.
     */
    Assert(pointer != NULL);
    Assert(pointer == (void *) MAXALIGN(pointer));

    /*
     * OK, it's probably safe to look at the chunk header.
     */
    context = ((StandardChunkHeader *)
               ((char *) pointer - STANDARDCHUNKHEADERSIZE))->context;

    AssertArg(MemoryContextIsValid(context));

    /* isReset must be false already */
    Assert(!context->isReset);

    /* TODO log iff failed */
    return (*context->methods->realloc) (context, pointer, size);
}

void
pgrest_util_sort(void *base, size_t n, size_t size,
                 int (*cmp)(const void *, const void *))
{
    unsigned char  *p1, *p2, *p;

    p = palloc(size);

    for (p1 = (unsigned char *) base + size;
         p1 < (unsigned char *) base + n * size;
         p1 += size)
    {
        memcpy(p, p1, size);

        for (p2 = p1;
             p2 > (unsigned char *) base && cmp(p2 - size, p) > 0;
             p2 -= size)
        {
            memcpy(p2, p2 - size, size);
        }

        memcpy(p2, p, size);
    }

    pfree(p);
}

evutil_socket_t
pgrest_util_mkstemp(const char *path)
{
    char            tmp[MAXPGPATH];
    evutil_socket_t fd;

    StrNCpy(tmp, path, sizeof(tmp));
    if ((fd = mkstemp(tmp)) == -1) {
        ereport(LOG, (errmsg(PGREST_PACKAGE " " "failed to create "
                             "temporary file: %s: %m", tmp)));
        return -1;
    }
    unlink(tmp);

    return fd;
}

int
pgrest_util_ncpu(void)
{
#ifndef WIN32
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
    SYSTEM_INFO si;

    GetNativeSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#endif
}
