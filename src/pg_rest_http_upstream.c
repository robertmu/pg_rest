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
    pgrest_http_upstream_srv_conf_t  *conf;
    pgrest_http_upstream_srv_conf_t **upstream;

    if (pgrest_http_upstreams == NULL) {
        pgrest_http_upstreams = pgrest_array_create(CurrentMemoryContext, 2,
                                     sizeof(pgrest_http_upstream_srv_conf_t *));
        if (pgrest_http_upstreams == NULL) {
            return NULL;
        }
    }

    conf = palloc0(sizeof(pgrest_http_upstream_srv_conf_t));
    conf->policy = PGREST_HTTP_UPSTREAM_UNKNOW;

    upstream = pgrest_array_push(pgrest_http_upstreams);
    if (upstream == NULL) {
        return NULL;
    }

   *upstream = conf;

    return conf;
}

static void *
pgrest_http_upstream_server_conf(void *parent)
{
    pgrest_http_upstream_server_t   *us;
    pgrest_http_upstream_srv_conf_t *upstream = parent;

    us = pgrest_array_push(upstream->servers);
    if (us == NULL) {
        return NULL;
    }

    MemSet(us, 0, sizeof(pgrest_http_upstream_server_t));

    return us;
}

static void
pgrest_http_upstream_name(pgrest_conf_command_t *cmd, 
                          void                  *val, 
                          void                  *parent)
{
    pgrest_http_upstream_srv_conf_t *conf = parent;
    char                            *value = val;
    pgrest_url_t                     u;

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

    conf->servers = pgrest_array_create(CurrentMemoryContext, 2,
                                     sizeof(pgrest_http_upstream_server_t));
    if (conf->servers == NULL) {
         ereport(ERROR, (errmsg(PGREST_PACKAGE " " "out of memory")));
    }
}

static void  
pgrest_http_upstream_algorithm(pgrest_conf_command_t *cmd, 
                               void                  *val, 
                               void                  *parent)
{
    pgrest_http_upstream_srv_conf_t *conf = parent;
    char                            *value = val;

    if (strcmp(value, "rw/split") == 0) {
        conf->policy = PGREST_HTTP_UPSTREAM_RWSPLIT;
    } else if (strcmp(value, "sharding") == 0) {
        conf->policy = PGREST_HTTP_UPSTREAM_SHARDING;
    } else {
        ereport(ERROR, 
                (errmsg(PGREST_PACKAGE " " "invalid value for directive \"%s"
                        "\": \"%s\" only accept \"rw/split\" or \"sharding\"",
                         cmd->name, value)));
    }
}

static void  
pgrest_http_upstream_saddress(pgrest_conf_command_t *cmd, 
                              void                  *val, 
                              void                  *parent)
{
    pgrest_url_t                   u;
    pgrest_http_upstream_server_t *server = parent;
    char                          *value = val;

    MemSet(&u, 0, sizeof(pgrest_url_t));

    u.url = value;
    u.url_len = strlen(value);
    u.default_port = 5432;

    if (!pgrest_inet_parse_url(&u)) {
        if (u.err) {
            ereport(ERROR, 
                    (errmsg(PGREST_PACKAGE " " "invalid value \"%s\" for dire"
                            "ctive \"%s\": \"%s\"", value, cmd->name, u.err)));
        }
    }

    pgrest_string_init(&server->name, u.url, u.url_len);
    server->addrs = u.addrs;
    server->naddrs = u.naddrs;
}

bool
pgrest_http_upstream_add(pgrest_url_t *u, 
                         pgrest_uint_t flags, 
                         pgrest_http_upstream_srv_conf_t **conf)
{
    pgrest_uint_t                     i;
    pgrest_http_upstream_server_t    *us;
    pgrest_http_upstream_srv_conf_t **upstreams;
    pgrest_http_upstream_srv_conf_t  *upstream = *conf;

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

    upstreams = pgrest_http_upstreams->elts;

    for (i = 0; i < pgrest_http_upstreams->size; i++) {

        if (upstreams[i]->host.len != u->host_len
            || strncasecmp((char *)upstreams[i]->host.base, u->host, u->host_len)
               != 0)
        {
            continue;
        }

        if ((flags & PGREST_HTTP_UPSTREAM_CREATE)
             && (upstreams[i]->flags & PGREST_HTTP_UPSTREAM_CREATE))
        {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "duplicate "
                            "upstream \"%s\"", u->host)));
            return false;
        }

        if ((upstreams[i]->flags & PGREST_HTTP_UPSTREAM_CREATE) && !u->no_port) {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "upstream \"%s\" may"
                             " not have port %d", u->host, u->port)));
            return false;
        }

        if ((flags & PGREST_HTTP_UPSTREAM_CREATE) && !upstreams[i]->no_port) {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "upstream \"%s\" may "
                            "not have port %d", u->host, upstreams[i]->port)));
            return false;
        }

        if (upstreams[i]->port && u->port
            && upstreams[i]->port != u->port)
        {
            continue;
        }

        if (upstreams[i]->default_port && u->default_port
            && upstreams[i]->default_port != u->default_port)
        {
            continue;
        }

        Assert(upstream == NULL);
        upstream = upstreams[i];
        return true;
    }

    if (upstream == NULL) {
        upstreams = pgrest_array_push(pgrest_http_upstreams);
        if (upstreams == NULL) {
            return false;
        }
        upstream = palloc0(sizeof(pgrest_http_upstream_srv_conf_t));
       *upstreams = upstream;
    }

    upstream->flags = flags;
    pgrest_string_init(&upstream->host, u->host, u->host_len);
    upstream->port = u->port;
    upstream->default_port = u->default_port;
    upstream->no_port = u->no_port;

    if (u->naddrs == 1 && (u->port || u->family == AF_UNIX)) {
        upstream->servers = pgrest_array_create(CurrentMemoryContext, 1,
                                         sizeof(pgrest_http_upstream_server_t));
        if (upstream->servers == NULL) {
            return false;
        }

        us = pgrest_array_push(upstream->servers);
        if (us == NULL) {
            return false;
        }

        MemSet(us, 0, sizeof(pgrest_http_upstream_server_t));
        us->addrs = u->addrs;
        us->naddrs = 1;
    }

    return true;
}

void
pgrest_http_upstream_conf_register(void)
{
    pgrest_conf_def_obj("upstream", pgrest_http_upstream_conf);
    pgrest_conf_def_obj("userver", pgrest_http_upstream_server_conf);

    pgrest_conf_def_cmd("name", 0, 0, 0, pgrest_http_upstream_name);
    pgrest_conf_def_cmd("algorithm", 0, 0, 0, pgrest_http_upstream_algorithm);
    pgrest_conf_def_cmd("uaddress", 0, 0, 0, pgrest_http_upstream_saddress);
}

bool pgrest_http_upstream_init(void)
{
    pgrest_uint_t                     i;
    pgrest_http_upstream_srv_conf_t **upstreams = pgrest_http_upstreams->elts;

    for (i = 0; i < pgrest_http_upstreams->size; i++) {
        if (upstreams[i]->servers->size == 0) {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "no servers are inside "
                            "upstream \"%s\"", upstreams[i]->host.base)));
            return false;
        }

        if (upstreams[i]->servers->size > 1 && 
            upstreams[i]->policy == PGREST_HTTP_UPSTREAM_UNKNOW) 
        {
            ereport(LOG, 
                    (errmsg(PGREST_PACKAGE " " "you must specify a valid value "
                            "for \"upstream->algorithm\" directive, only accept"
                            " \"rw/split\" or \"sharding\"")));
            return false;
        }
    }

    return true;
}
