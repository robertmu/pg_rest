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

static pgrest_guc_command_t  pgrest_guc_commands[] = {

    { PGREST_PACKAGE ".configure_file",
      gettext_noop("Sets configure file path of pg_rest."),
      PGC_STRING,
      offsetof(pgrest_setting_t, configure_file),
      (intptr_t) "pg_rest.json",
      0,
      0,
      PGC_POSTMASTER,
      GUC_NOT_IN_SAMPLE,
      NULL },

    { NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }
};

pgrest_setting_t  pgrest_setting;

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
    pgrest_setting_t      *s = (pgrest_setting_t *) setting;
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

    /* parse configure file */
    if(!pgrest_conf_parse(s, s->configure_file)) {
        ereport(ERROR,
                (errmsg(PGREST_PACKAGE " " "parse configure file failed")));
    }

#if (PGREST_DEBUG)
    pgrest_guc_info_print(setting);
#endif
}
