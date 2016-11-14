/* -------------------------------------------------------------------------
 *
 * pq_rest_http_upstream.c
 *   http upstream routines.
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http_upstream.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

static pgrest_array_t *pgrest_http_upstreams = NULL;

static void *
pgrest_http_upstream_conf(void *parent)
{
    pgrest_http_upstream_conf_t *conf;

    if (pgrest_http_upstreams == NULL) {
        pgrest_http_upstreams = pgrest_array_create(2, 
                                     sizeof(pgrest_http_upstream_conf_t));
    }

    conf = pgrest_array_push(pgrest_http_upstreams);

    MemSet(conf, 0, sizeof(pgrest_http_upstream_conf_t));

    return conf;
}

static void *
pgrest_http_upstream_server_conf(void *parent)
{



    return NULL;
}

static void
pgrest_http_upstream_name(pgrest_conf_command_t *cmd, 
                          void                  *val, 
                          void                  *parent)
{
    pgrest_http_upstream_conf_t *conf  = parent;
    char                        *value = val;
    pgrest_url_t                 u;

    MemSet(&u, 0, sizeof(pgrest_url_t));

    u.host = value;
    u.host_len = strlen(value);
    u.no_resolve = 1;
    u.no_port = 1;

    if(!pgrest_http_upstream_add(&u, PGREST_HTTP_UPSTREAM_CREATE, &conf)) {
         ereport(ERROR, 
                 (errmsg(PGREST_PACKAGE " " "invalid value for "
                         "directive \"%s\": \"%s\"", cmd->name, value)));
    }


}

static void  
pgrest_http_upstream_algorithm(pgrest_conf_command_t *cmd, 
                               void                  *val, 
                               void                  *parent)
{

}

static void  
pgrest_http_upstream_saddress(pgrest_conf_command_t *cmd, 
                              void                  *val, 
                              void                  *parent)
{

}

static void  
pgrest_http_upstream_master(pgrest_conf_command_t *cmd, 
                            void                  *val, 
                            void                  *parent)
{

}

bool
pgrest_http_upstream_add(pgrest_url_t *u, 
                         pgrest_uint_t flags, 
                         pgrest_http_upstream_conf_t **conf)
{
    pgrest_uint_t                   i;
    pgrest_http_upstream_conf_t   **upstream;

    if (!(flags & PGREST_HTTP_UPSTREAM_CREATE)) {

        if (!pgrest_inet_parse_url(u)) {
            if (u->err) {
                ereport(LOG, 
                        (errmsg(PGREST_PACKAGE " " "%s in "
                                "upstream \"%s\"", u->err, u->url)));
            }

            return false;
        }
    }

    upstream = pgrest_http_upstreams->elts;

    for (i = 0; i < pgrest_http_upstreams->size; i++) {

        if (upstream[i]->host.len != u->host_len
            || strncasecmp((char *)upstream[i]->host.base, u->host, u->host_len)
               != 0)
        {
            continue;
        }

        if ((flags & PGREST_HTTP_UPSTREAM_CREATE)
             && (upstream[i]->flags & PGREST_HTTP_UPSTREAM_CREATE))
        {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "duplicate "
                            "upstream \"%s\"", u->host)));
            return false;
        }

        if ((upstream[i]->flags & PGREST_HTTP_UPSTREAM_CREATE) && !u->no_port) {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "upstream \"%s\" may "
                            "not have port %d", u->host, u->port)));
            return false;
        }

        if ((flags & PGREST_HTTP_UPSTREAM_CREATE) && !upstream[i]->no_port) {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "upstream \"%s\" may "
                            "not have port %d", u->host, upstream[i]->port)));
            return false;
        }

        if (upstream[i]->port && u->port
            && upstream[i]->port != u->port)
        {
            continue;
        }

        if (upstream[i]->default_port && u->default_port
            && upstream[i]->default_port != u->default_port)
        {
            continue;
        }

        if (flags & PGREST_HTTP_UPSTREAM_CREATE) {
            upstream[i]->flags = flags;
        }

        return true;

    }

    /* XXX need to finish */
    return true;
}

void
pgrest_http_upstream_conf_register(void)
{
    pgrest_conf_def_obj("upstream", pgrest_http_upstream_conf);
    pgrest_conf_def_obj("upstream_server", pgrest_http_upstream_server_conf);

    pgrest_conf_def_cmd("name", 0, 0, 0, pgrest_http_upstream_name);
    pgrest_conf_def_cmd("algorithm", 0, 0, 0, pgrest_http_upstream_algorithm);
    pgrest_conf_def_cmd("server_address", 0, 0, 0, pgrest_http_upstream_saddress);
    pgrest_conf_def_cmd("master", 0, 0, 0, pgrest_http_upstream_master);
}
