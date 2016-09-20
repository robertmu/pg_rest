/* -------------------------------------------------------------------------
 *
 * pq_rest_http.c
 *   http 1.0,1.1 handler routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

typedef bool (*pgrest_addr_split_pt)(char *rawstring,
                                     char separator,
                                     List **namelist);

void pgrest_http_conn_init(pgrest_connection_t *conn)
{

}

static List *
pgrest_http_listener_prepare(int family, 
                        const char *listen_addresses,
                        int port_number,
                        pgrest_addr_split_pt split,
                        const char *conf_name,
                        pgrest_conn_handler_pt conn_handler)
{
    char        *raw_string;
    List        *elem_list;
    ListCell    *l;
    List        *listeners = NIL;
    List        *result = NIL;

    Assert(listen_addresses != NULL);

    /* Need a modifiable copy of listen_addresses */
    raw_string = pstrdup(listen_addresses);

    /* Parse string into list of hostnames */
    if (!split(raw_string, ',', &elem_list)) {
        /* syntax error in list */
        ereport(LOG,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg(PGREST_PACKAGE " " "invalid list syntax in parameter \"%s\"",
                         conf_name)));
        return result;
    }

    foreach(l, elem_list) {
        char *cur = (char *) lfirst(l);

        switch (family) {
#ifdef HAVE_UNIX_SOCKETS
        case AF_UNIX:
            listeners = pgrest_listener_add(AF_UNIX, NULL,
                                            (unsigned short)port_number, 
                                             cur,conn_handler);
            result = list_concat(result, listeners);
            break;
#endif

        default: /* AF_INET */
            if (strcmp(cur, "*") == 0) {
                listeners = pgrest_listener_add(AF_UNSPEC, NULL,
                                                (unsigned short)port_number,
                                                 NULL,conn_handler);
            } else {
                listeners = pgrest_listener_add(AF_UNSPEC, cur,
                                                (unsigned short)port_number,
                                                 NULL,conn_handler);
            }
            result = list_concat(result, listeners);
        }
	}

    list_free(elem_list);
    pfree(raw_string);

    return result;
}

static List *
pgrest_http_listener_init(void *data)
{
    pgrest_conn_handler_pt conn_handler;
    List                  *listeners = NIL;
    List                  *result = NIL;

    conn_handler = data;

    if (pgrest_setting.http_enabled) {
        listeners = pgrest_http_listener_prepare(AF_UNSPEC, 
                                            pgrest_setting.listener_addresses,
                                            pgrest_setting.http_port, 
                                            SplitIdentifierString,
                                            PGREST_PACKAGE ".listen_addresses",
                                            conn_handler);
        result = list_concat(result, listeners);
    }

#ifdef HAVE_OPENSSL
    if (pgrest_setting.https_enabled) {
        listeners = pgrest_http_listener_prepare(AF_UNSPEC, 
                                            pgrest_setting.listener_addresses,
                                            pgrest_setting.https_port, 
                                            SplitIdentifierString,
                                            PGREST_PACKAGE ".listen_addresses",
                                            conn_handler);
        result = list_concat(result, listeners);
    }
#endif

#ifdef HAVE_UNIX_SOCKETS
    listeners = pgrest_http_listener_prepare(AF_UNIX, 
                                        pgrest_setting.listener_unix_socket_dirs,
                                        pgrest_setting.http_port, 
                                        SplitDirectoriesString,
                                        PGREST_PACKAGE ".unix_socket_dirs",
                                        conn_handler);
    result = list_concat(result, listeners);
#endif

    return result;
}

void pgrest_http_init(void)
{
    pgrest_listener_hook_add(pgrest_http_listener_init, pgrest_http_conn_init);
}
