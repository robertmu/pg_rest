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

const char *
pgrest_util_inet_ntop(struct sockaddr *sa, socklen_t socklen, 
                      char *dst, size_t size)
{
    switch (sa->sa_family) {
    case AF_INET:
        return inet_net_ntop(AF_INET,
                             &((struct sockaddr_in *) sa)->sin_addr.s_addr,
                             32, dst, size);
#ifdef HAVE_IPV6
    case AF_INET6:
        return inet_net_ntop(AF_INET6,
                             &((struct sockaddr_in6 *) sa)->sin6_addr.s6_addr,
                             128, dst, size);
#endif
#ifdef HAVE_UNIX_SOCKETS
    case AF_UNIX:
        if (socklen <= (socklen_t) offsetof(struct sockaddr_un, sun_path)) {
            snprintf(dst, size, "????");
        } else {
            snprintf(dst, size, "%s", ((struct sockaddr_un *) sa)->sun_path);
        }

        return dst;
#endif
    default:
        ereport(WARNING,
                (errmsg(PGREST_PACKAGE " " "unrecognized address family %d",sa->sa_family)));
        return NULL;
    }
}

void *
pgrest_util_palloc(Size size)
{
    /* duplicates MemoryContextAlloc to avoid increased overhead */
	void	   *ret;

    AssertArg(MemoryContextIsValid(CurrentMemoryContext));

    if (!AllocSizeIsValid(size)) {
        elog(LOG, PGREST_PACKAGE " " "invalid memory alloc request size %zu", size);
        return NULL;
    }

    CurrentMemoryContext->isReset = false;

    ret = (*CurrentMemoryContext->methods->alloc) (CurrentMemoryContext, size);
    if (ret == NULL) {
        MemoryContextStats(TopMemoryContext);
        ereport(LOG,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg(PGREST_PACKAGE " " "out of memory"),
                 errdetail("Failed on request of size %zu.", size)));
    }

    return ret;
}

/*
 * repalloc
 *      Adjust the size of a previously allocated chunk.
 */
void *
pgrest_util_repalloc(void *pointer, Size size)
{
    MemoryContext  context;
    void          *ret;

    if (!AllocSizeIsValid(size)) {
        elog(LOG, PGREST_PACKAGE " " "invalid memory alloc request size %zu", size);
        return NULL;
    }

    if (pointer == NULL) {
        return pgrest_util_palloc(size);
    }

    if (size == 0) {
        pgrest_util_pfree(pointer);
        return pgrest_util_palloc(0);
    }

    /*
     * Try to detect bogus pointers handed to us, poorly though we can.
     * Presumably, a pointer that isn't MAXALIGNED isn't pointing at an
     * allocated chunk.
     */
    Assert(pointer == (void *) MAXALIGN(pointer));

    /*
     * OK, it's probably safe to look at the chunk header.
     */
    context = ((StandardChunkHeader *)
               ((char *) pointer - STANDARDCHUNKHEADERSIZE))->context;

    AssertArg(MemoryContextIsValid(context));

    /* isReset must be false already */
    Assert(!context->isReset);

	ret = (*context->methods->realloc) (context, pointer, size);
    if (ret == NULL) {
        MemoryContextStats(TopMemoryContext);
        ereport(LOG,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg(PGREST_PACKAGE " " "out of memory"),
                 errdetail("Failed on request of size %zu.", size)));
	}

	return ret;
}

void
pgrest_util_pfree(void *pointer)
{
    pfree(pointer);
}

void  
pgrest_util_event_log(int severity, const char *msg)
{
    int         level;

    switch (severity) {
    case EVENT_LOG_WARN:
        level = WARNING;
        break;
    case EVENT_LOG_ERR:
        level = WARNING;
        break;
    default:
        level = 0;
    }

    if (level) {
        ereport(level, (errmsg(PGREST_PACKAGE " " "worker %d event internal "
                               "error: %s", pgrest_worker_index, msg)));
    }
}

void  
pgrest_util_event_fatal(int err)
{
    ereport(ERROR, (errmsg(PGREST_PACKAGE " " "worker %d event fatal "
                           "error: %d", pgrest_worker_index, err)));
}
