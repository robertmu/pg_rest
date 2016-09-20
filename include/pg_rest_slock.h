/*-------------------------------------------------------------------------
 *
 * include/pg_rest_slock.h
 *
 * spin lock.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_SLOCK_H
#define PG_REST_SLOCK_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

#define S_TRYLOCK(lock)             TAS_SPIN(lock)
#define SpinTryLockAcquire(lock)    S_TRYLOCK(lock)

#endif /* PG_REST_SLOCK_H */
