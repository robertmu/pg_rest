/* -------------------------------------------------------------------------
 *
 * pq_rest_guc.c
 *   pg guc configure
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_guc.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct {
    const char           *name;
    const char           *desc;
    enum config_type      type;
    size_t                offset;
    intptr_t              boot_value;
    int                   min_value;
    int                   max_value;
    GucContext            context;
    int                   flags;
    void                 *check_hook;
} pgrest_guc_command_t;

static bool pgrest_guc_reuse_check(bool *newval, void **extra, GucSource source);
static bool pgrest_guc_https_check(bool *newval, void **extra, GucSource source);

static pgrest_guc_command_t  pgrest_guc_commands[] = {

    { PGREST_PACKAGE ".listen_addresses",
      gettext_noop("Sets the host name or IP address(es) to listen to."),
      PGC_STRING,
      offsetof(pgrest_setting_t, listener_addresses),
      (intptr_t) "localhost",
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE | GUC_LIST_INPUT,
      NULL },

    { PGREST_PACKAGE ".http_enabled",
      gettext_noop("Whether to enable listening for client requests on an HTTP port."),
      PGC_BOOL,
      offsetof(pgrest_setting_t, http_enabled),
      true,
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".http_port",
      gettext_noop("Network port to listen on for HTTP requests."),
      PGC_INT,
      offsetof(pgrest_setting_t, http_port),
      8080,
      1,
      65535,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".crud_url_prefix",
      gettext_noop("URL prefix of the CRUD (Create-Read-Update-Delete) endpoint."),
      PGC_STRING,
      offsetof(pgrest_setting_t, crud_url_prefix),
      (intptr_t) "/crud/",
      0,
      0,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".doc_url_prefix",
      gettext_noop("URL prefix of the JSON Document (DOC) endpoint."),
      PGC_STRING,
      offsetof(pgrest_setting_t, doc_url_prefix),
      (intptr_t) "/doc/",
      0,
      0,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".sql_url_prefix",
      gettext_noop("URL prefix used for SQL endpoint (user API)."),
      PGC_STRING,
      offsetof(pgrest_setting_t, sql_url_prefix),
      (intptr_t) "/sql/",
      0,
      0,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".https_enabled",
      gettext_noop("Whether to enable listening for client requests on an HTTPS(SSL) port."),
      PGC_BOOL,
      offsetof(pgrest_setting_t, https_enabled),
      false,
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      pgrest_guc_https_check },

    { PGREST_PACKAGE ".https_port",
      gettext_noop("Network port to listen on for HTTPS(SSL) requests."),
      PGC_INT,
      offsetof(pgrest_setting_t, https_port),
      443,
      1,
      65535,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".https_ssl_key",
      gettext_noop("SSL key file to use for HTTPS (SSL) connections."),
      PGC_STRING,
      offsetof(pgrest_setting_t, https_ssl_key),
      (intptr_t) "sslkey.pem",
      0,
      0,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".worker_processes",
      gettext_noop("Number of worker processes."),
      PGC_INT,
      offsetof(pgrest_setting_t, worker_processes),
      4,
      1,
      PGREST_MAX_WORKERS,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".listen_backlog",
      gettext_noop("Defines the maximum length to which the queue of pending connections."),
      PGC_INT,
      offsetof(pgrest_setting_t, listener_backlog),
      DEFAUlT_PGREST_LISTEN_BACKLOG,
      1,
      65535,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".tcp_rcvbuf",
      gettext_noop("Receive buffer size (the SO_RCVBUF option) for the listening socket."),
      PGC_INT,
      offsetof(pgrest_setting_t, listener_tcp_rcvbuf),
      -1,
      -1,
      65535,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".tcp_sndbuf",
      gettext_noop("Send buffer size (the SO_SNDBUF option) for the listening socket."),
      PGC_INT,
      offsetof(pgrest_setting_t, listener_tcp_sndbuf),
      -1,
      -1,
      65535,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".tcp_keepalive",
      gettext_noop("Configure the TCP keepalive behavior for the listening socket."),
      PGC_STRING,
      offsetof(pgrest_setting_t, listener_tcp_keepalive),
      (intptr_t) "",
      0,
      0,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".unix_socket_dirs",
      gettext_noop("Sets the directories where Unix-domain sockets will be created."),
      PGC_STRING,
      offsetof(pgrest_setting_t, listener_unix_socket_dirs),
#ifdef HAVE_UNIX_SOCKETS
      (intptr_t) DEFAULT_PGREST_SOCKET_DIR,
#else
      (intptr_t) "",
#endif
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE | GUC_LIST_INPUT,
      NULL },

    { PGREST_PACKAGE ".tcp_keepintvl",
      gettext_noop("Time between TCP keepalive retransmits."),
      PGC_INT,
      offsetof(pgrest_setting_t, listener_tcp_keepintvl),
      0,
      0,
      INT_MAX,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".tcp_keepidle",
      gettext_noop("Time between issuing TCP keepalives."),
      PGC_INT,
      offsetof(pgrest_setting_t, listener_tcp_keepidle),
      0,
      0,
      INT_MAX,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".tcp_keepcnt",
      gettext_noop("Maximum number of TCP keepalive retransmits."),
      PGC_INT,
      offsetof(pgrest_setting_t, listener_tcp_keepcnt),
      0,
      0,
      INT_MAX,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".listen_ipv6only",
      gettext_noop("Whether an IPv6 socket listening on a wildcard address [::] "
                   "will accept only IPv6 connections or both IPv6 and IPv4 connections."),
      PGC_BOOL,
      offsetof(pgrest_setting_t, listener_ipv6only),
      true,
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".accept_filter",
      gettext_noop("Set the name of accept filter (the SO_ACCEPTFILTER option) "
                   "for the listening socket that filters incoming connections "
                   "before passing them to accept()."),
      PGC_STRING,
      offsetof(pgrest_setting_t, acceptor_accept_filter),
      (intptr_t) "",
      0,
      0,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".deferred_accept",
      gettext_noop("Whether to use a deferred accept() "
                   "(the TCP_DEFER_ACCEPT socket option) on Linux."),
      PGC_BOOL,
      offsetof(pgrest_setting_t, acceptor_deferred_accept),
      false,
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".setfib",
      gettext_noop("Set the associated routing table, "
                   "FIB (the SO_SETFIB option) for the listening socket."),
      PGC_INT,
      offsetof(pgrest_setting_t, acceptor_setfib),
      -1,
      -1,
      INT_MAX,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".fastopen",
      gettext_noop("Enables TCP Fast Open for the listening socket (1.5.8) "
                   "and limits the maximum length for the queue of connections "
                   "that have not yet completed the three-way handshake."),
      PGC_INT,
      offsetof(pgrest_setting_t, acceptor_fastopen),
      -1,
      -1,
      INT_MAX,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".reuseport",
      gettext_noop("Enables create an individual listening socket for "
                   "each worker process (using the SO_REUSEPORT socket option)."),
      PGC_BOOL,
      offsetof(pgrest_setting_t, acceptor_reuseport),
      false,
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      pgrest_guc_reuse_check },

    { PGREST_PACKAGE ".worker_priority",
      gettext_noop("Defines the scheduling priority for worker processes "
                   "like it is done by the nice command."),
      PGC_INT,
      offsetof(pgrest_setting_t, worker_priority),
      0,
      -20,
      20,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".worker_connections",
      gettext_noop("Sets the maximum number of simultaneous connections "
                   "that can be opened by a worker process."),
      PGC_INT,
      offsetof(pgrest_setting_t, worker_noconn),
      1024,
      1,
      65535,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".worker_nofile",
      gettext_noop("Changes the limit on the maximum number of open "
                   "files (RLIMIT_NOFILE) for worker processes."),
      PGC_INT,
      offsetof(pgrest_setting_t, worker_nofile),
      -1,
      -1,
      65535,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".accept_mutex",
      gettext_noop("If accept_mutex is enabled, worker processes "
                   "will accept new connections by turn."),
      PGC_BOOL,
      offsetof(pgrest_setting_t, acceptor_mutex),
      true,
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".accept_mutex_delay",
      gettext_noop("If accept_mutex is enabled, specifies the maximum "
                   "time during which a worker process will try "
                   "to restart accepting new connections."),
      PGC_INT,
      offsetof(pgrest_setting_t, acceptor_mutex_delay),
      500,
      200,
      1000,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".client_header_timeout",
      gettext_noop("Defines a timeout for reading client request header."),
      PGC_INT,
      offsetof(pgrest_setting_t, acceptor_timeout),
      60000,
      1000,
      600000,
      PGC_SUSET,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { PGREST_PACKAGE ".multi_accept",
      gettext_noop("If multi_accept is disabled, a worker process "
                   "will accept one new connection at a time."),
      PGC_BOOL,
      offsetof(pgrest_setting_t, acceptor_multi_accept),
      false,
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }
};


pgrest_setting_t  pgrest_setting;


static bool
pgrest_guc_reuse_check(bool *newval, void **extra, GucSource source)
{
	if (*newval == true) {
#ifndef HAVE_REUSEPORT
        GUC_check_errdetail(PGREST_PACKAGE " " "reuseport is not supported "
                            "on this platform");
        return false;            
#endif
    }

    return true;
}

static bool
pgrest_guc_https_check(bool *newval, void **extra, GucSource source)
{
	if (*newval == true) {
#ifndef HAVE_OPENSSL
        GUC_check_errdetail(PGREST_PACKAGE " " "build without openssl "
                            "please rebuild " PGREST_PACKAGE " --with-openssl");
        return false;            
#endif
    }

    return true;
}

static StringInfo
pgrest_guc_config_format(char *setting)
{
    pgrest_guc_command_t  *cmd;
    StringInfo             result;

    result = makeStringInfo();

    for ( cmd = pgrest_guc_commands ; cmd->name; cmd++) {

        switch (cmd->type) {
        case PGC_INT:
            appendStringInfo(result,
                "!\t%-30s %d\n", cmd->name, *((int *) (setting + cmd->offset)));
            break;
        case PGC_BOOL:
            appendStringInfo(result,
                "!\t%-30s %s\n", cmd->name, *((bool *) (setting + cmd->offset)) ? "on" : "off");
            break;
        case PGC_STRING:
            appendStringInfo(result,
                "!\t%-30s %s\n", cmd->name, *((char **) (setting + cmd->offset)));
            break;
        default:
            Assert(false);
        }
    }

    return result;
}

void
pgrest_guc_info_print(char *setting)
{
    StringInfo buffer;

    buffer = pgrest_guc_config_format(setting);

    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print setting ++++++++++")));
    ereport(DEBUG1,(errmsg("\n%s", buffer->data)));
    ereport(DEBUG1,(errmsg(PGREST_PACKAGE " " "print setting ----------")));

    pfree(buffer->data);
    pfree(buffer);
}

void
pgrest_guc_init(char *setting)
{
    pgrest_guc_command_t  *cmd;

    for ( cmd = pgrest_guc_commands ; cmd->name; cmd++) {

        switch (cmd->type) {
        case PGC_INT:
            DefineCustomIntVariable(cmd->name,
                                    cmd->desc,
                                    NULL,
                                    (int *) (setting + cmd->offset),
                                    cmd->boot_value,
                                    cmd->min_value,
                                    cmd->max_value,
                                    cmd->context,
                                    cmd->flags,
                                    cmd->check_hook, 
                                    NULL, 
                                    NULL);
            break;
        case PGC_BOOL:
            DefineCustomBoolVariable(cmd->name,
                                     cmd->desc,
                                     NULL,
                                     (bool *) (setting + cmd->offset),
                                     cmd->boot_value,
                                     cmd->context,
                                     cmd->flags,
                                     cmd->check_hook, 
                                     NULL, 
                                     NULL);

            break;
        case PGC_STRING:
            DefineCustomStringVariable(cmd->name,
                                       cmd->desc,
                                       NULL,
                                       (char **) (setting + cmd->offset),
                                       (const char *)cmd->boot_value,
                                       cmd->context,
                                       cmd->flags,
                                       cmd->check_hook, 
                                       NULL, 
                                       NULL);
            break;
        default:
            Assert(false);
        }
    }

    EmitWarningsOnPlaceholders(PGREST_PACKAGE);
#if (PGREST_DEBUG)
    pgrest_guc_info_print(setting);
#endif
}
