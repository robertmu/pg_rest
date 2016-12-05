/* -------------------------------------------------------------------------
 *
 * pq_rest_http_push_stream.c
 *   http push stream module
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http_push_stream.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

#include "pg_rest_http_push_stream.h"

typedef enum {
    PGREST_HTTP_PUSH_STREAM_PUB = 0,
    PGREST_HTTP_PUSH_STREAM_SUB,
    PGREST_HTTP_PUSH_STREAM_ADMIN,
    PGREST_HTTP_PUSH_STREAM_UNKNOW
} pgrest_http_push_stream_mode_e;

typedef enum {
    PGREST_HTTP_PUSH_STREAM_WEBSOCKET = 0,
    PGREST_HTTP_PUSH_STREAM_LONGPOLLING,
    PGREST_HTTP_PUSH_STREAM_POLLING,
} pgrest_http_push_stream_tunnel_e;

typedef struct {
    pgrest_string_t                    param_name;
    bool                               store_message;
} pgrest_http_push_stream_pub_t;

typedef struct {
    pgrest_http_push_stream_tunnel_e   tunnel;
} pgrest_http_push_stream_sub_t;

typedef struct {
} pgrest_http_push_stream_admin_t;

typedef struct {
    pgrest_http_push_stream_mode_e      mode;
    union {
        pgrest_http_push_stream_pub_t   pub_conf;
        pgrest_http_push_stream_sub_t   sub_conf;
        pgrest_http_push_stream_admin_t admin_conf;
    } value;
} pgrest_http_push_stream_conf_t;

static int
pgrest_http_push_stream_handler(pgrest_http_handler_t  *self,
                                pgrest_http_request_t  *req);
static bool pgrest_http_push_stream_init(pgrest_http_handler_t *self);
static void pgrest_http_push_stream_fini(pgrest_http_handler_t *self);


static void *
pgrest_http_push_stream_conf(void *parent)
{
    pgrest_http_push_stream_conf_t     *conf;
    pgrest_http_push_stream_handler_t  *handler;
    pgrest_conf_http_path_t            *path = parent;

    /* set default value */
    conf = palloc0(sizeof(pgrest_http_push_stream_conf_t));
    conf->mode = PGREST_HTTP_PUSH_STREAM_UNKNOW;

    handler = (void*)pgrest_http_handler_create(path, sizeof(*handler));
    if (handler == NULL) {
        return NULL;
    }

    handler->super.name = "push_stream";
    handler->super.init = pgrest_http_push_stream_init;
    handler->super.fini = pgrest_http_push_stream_fini;
    handler->super.handler = pgrest_http_push_stream_handler;
    handler->conf = conf;

    return conf;
}

static void  
pgrest_http_push_stream_mode(pgrest_conf_command_t *cmd, 
                             void                  *val, 
                             void                  *parent)
{
    pgrest_http_push_stream_conf_t *conf  = parent;
    char                           *value = val;

    if (strcmp(value, "pub") == 0) {
        conf->mode = PGREST_HTTP_PUSH_STREAM_PUB;
    } else if (strcmp(value, "sub") == 0) {
        conf->mode = PGREST_HTTP_PUSH_STREAM_SUB;
    } else if (strcmp(value, "admin") == 0) {
        conf->mode = PGREST_HTTP_PUSH_STREAM_ADMIN;
    } else {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value for directive \"%s\""
                         ": \"%s\" only accept \"pub\" or \"sub\" or \"admin\"",
                         cmd->name, value)));
    }
}

static void  
pgrest_http_push_stream_name(pgrest_conf_command_t *cmd, 
                             void                  *val, 
                             void                  *parent)
{
    pgrest_http_push_stream_conf_t *conf  = parent;
    char                           *value = val;

    conf->value.pub_conf.param_name.base = (unsigned char *)pstrdup(value);
    conf->value.pub_conf.param_name.len = strlen(value);
}

static void  
pgrest_http_push_stream_stor(pgrest_conf_command_t *cmd, 
                             void                  *val, 
                             void                  *parent)
{
    pgrest_http_push_stream_conf_t *conf = parent;
    bool                           *newval = val;

    conf->value.pub_conf.store_message = *newval;
}

static void  
pgrest_http_push_stream_tunnel(pgrest_conf_command_t *cmd, 
                               void                  *val, 
                               void                  *parent)
{
    pgrest_http_push_stream_conf_t *conf  = parent;
    char                           *value = val;

    if (strcmp(value, "websocket") == 0) {
        conf->mode = PGREST_HTTP_PUSH_STREAM_WEBSOCKET;
    } else if (strcmp(value, "long-polling") == 0) {
        conf->mode = PGREST_HTTP_PUSH_STREAM_LONGPOLLING;
    } else if (strcmp(value, "polling") == 0) {
        conf->mode = PGREST_HTTP_PUSH_STREAM_POLLING;
    } else {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value for directive"
                        " \"%s\": \"%s\" only accept \"websocket\" or "
                        "\"long-polling\" or \"polling\"", cmd->name, value)));
    }
}

static bool 
pgrest_http_push_stream_init(pgrest_http_handler_t *self)
{
    pgrest_http_push_stream_handler_t *handler = (void *)self;
    pgrest_http_push_stream_conf_t    *conf = handler->conf;

    switch (conf->mode) {
    case PGREST_HTTP_PUSH_STREAM_PUB:
        if (!conf->value.pub_conf.param_name.base) {
            pgrest_string_set(&conf->value.pub_conf.param_name, "id");
        }
        break;
    case PGREST_HTTP_PUSH_STREAM_SUB:
    case PGREST_HTTP_PUSH_STREAM_ADMIN:
        break;
    default:
         ereport(LOG, 
                (errmsg(PGREST_PACKAGE " " "you must specify a valid value"
                        " for \"push_stream->mode\" directive, only accept"
                        " \"pub\" or \"sub\" or \"admin\"")));
        return false;
    }

    return true;
}

static int
pgrest_http_push_stream_pub(pgrest_http_push_stream_handler_t *handler,
                            pgrest_http_request_t *req)
{
    return 0;
}

static int
pgrest_http_push_stream_sub(pgrest_http_push_stream_handler_t *handler,
                            pgrest_http_request_t *req)
{
    return 0;
}

static int
pgrest_http_push_stream_admin(pgrest_http_push_stream_handler_t *handler,
                            pgrest_http_request_t *req)
{
    return 0;
}

static void 
pgrest_http_push_stream_fini(pgrest_http_handler_t *self)
{

}

static int
pgrest_http_push_stream_handler(pgrest_http_handler_t *self,
                                pgrest_http_request_t *req)
{
    pgrest_http_push_stream_handler_t *handler = (void *)self;
    pgrest_http_push_stream_conf_t    *conf = handler->conf;

    switch (conf->mode) {
    case PGREST_HTTP_PUSH_STREAM_PUB:
        pgrest_http_push_stream_pub(handler, req);
        break;
    case PGREST_HTTP_PUSH_STREAM_SUB:
        pgrest_http_push_stream_sub(handler, req);
        break;
    case PGREST_HTTP_PUSH_STREAM_ADMIN:
        pgrest_http_push_stream_admin(handler, req);
        break;
    default:
        Assert(false);
    }

    return 0;
}

void
pgrest_http_push_stream_conf_register(void)
{
    pgrest_conf_def_obj("push_stream", pgrest_http_push_stream_conf);

    pgrest_conf_def_cmd("mode", 0, 0, 0, pgrest_http_push_stream_mode);
    pgrest_conf_def_cmd("param_name", 0, 0, 0, pgrest_http_push_stream_name);
    pgrest_conf_def_cmd("store_message", 0, 0, 0, pgrest_http_push_stream_stor);
    pgrest_conf_def_cmd("subscriber", 0, 0, 0, pgrest_http_push_stream_tunnel);
}
