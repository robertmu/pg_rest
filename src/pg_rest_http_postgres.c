/* -------------------------------------------------------------------------
 *
 * pq_rest_http_postgres.c
 *   http postgres module
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http_postgres.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

#include "pg_rest_http_postgres.h"

typedef struct {
    pgrest_string_t  dbname;
    pgrest_string_t  encode;
} pgrest_http_postgres_conf_t;

static int
pgrest_http_postgres_handler(pgrest_http_handler_t  *self,
                             pgrest_http_request_t  *req);
static bool pgrest_http_postgres_init(pgrest_http_handler_t *self);
static void pgrest_http_postgres_fini(pgrest_http_handler_t *self);


static void *
pgrest_http_postgres_conf(void *parent)
{
    pgrest_http_postgres_conf_t    *conf;
    pgrest_http_postgres_handler_t *handler;
    pgrest_conf_http_path_t        *path = parent;

    /* set default value */
    conf = palloc0(sizeof(pgrest_http_postgres_conf_t));

    handler = (void*)pgrest_http_handler_create(path, sizeof(*handler));
    handler->super.name = "postgres";
    handler->super.init = pgrest_http_postgres_init;
    handler->super.fini = pgrest_http_postgres_fini;
    handler->super.handler = pgrest_http_postgres_handler;
    handler->conf = conf;

    return conf;
}

static void  
pgrest_http_postgres_pgpass(pgrest_conf_command_t *cmd, 
                            void                  *val, 
                            void                  *parent)
{

}

static void  
pgrest_http_postgres_encode(pgrest_conf_command_t *cmd, 
                            void                  *val, 
                            void                  *parent)
{

}

static void  
pgrest_http_postgres_dbname(pgrest_conf_command_t *cmd, 
                            void                  *val, 
                            void                  *parent)
{

}

static bool 
pgrest_http_postgres_init(pgrest_http_handler_t *self)
{

    return true;
}

static void 
pgrest_http_postgres_fini(pgrest_http_handler_t *self)
{

}

static int
pgrest_http_postgres_handler(pgrest_http_handler_t *self,
                             pgrest_http_request_t *req)
{

    return 0;
}

void
pgrest_http_postgres_conf_register(void)
{
    pgrest_conf_def_obj("postgres", pgrest_http_postgres_conf);

    pgrest_conf_def_cmd("pg_pass", 0, 0, 0, pgrest_http_postgres_pgpass);
    pgrest_conf_def_cmd("dbname", 0, 0, 0, pgrest_http_postgres_dbname);
    pgrest_conf_def_cmd("encode", 0, 0, 0, pgrest_http_postgres_encode);
}
