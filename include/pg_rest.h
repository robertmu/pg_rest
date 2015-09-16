/*-------------------------------------------------------------------------
 *
 * include/pg_rest.h
 *
 * Declarations for public functions and types needed by the pg_rest extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_H
#define PG_REST_H


/* extension name used to determine if extension has been created */
#define PG_REST_EXTENSION_NAME "pg_rest"


/* function declarations for extension loading and unloading */
extern void _PG_init(void);
extern void _PG_fini(void);


#endif /* PG_REST_H */
