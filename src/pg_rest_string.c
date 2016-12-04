/* -------------------------------------------------------------------------
 *
 * pq_rest_string.c
 *   string mgmt routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_string.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

int 
pgrest_atoi(const char *arg, size_t len)
{
    char buff[32];

    memcpy(buff, arg, len);
    buff[len] = '\0';

    return atoi(buff);
}

char *
pgrest_strlchr(char *p, char *last, char c)
{
    while (p < last) {

        if (*p == c) {
            return p;
        }

        p++;
    }

    return NULL;
}

int
pgrest_count_params(pgrest_string_t path)
{
    int i, n = 0;

    for (i = 0; i< path.len; i++) {
        if (path.base[i] != ':' && path.base[i] != '*') {
            continue;
        }

        n++;
    }

    if (n >= 255) {
        n = 255;
    }

    return n;
}

off_t
pgrest_atoof(unsigned char *line, size_t n)
{
    off_t  value, cutoff, cutlim;

    if (n == 0) {
        return PGREST_ERROR;
    }

    cutoff = PGREST_MAX_OFF_T_VALUE / 10;
    cutlim = PGREST_MAX_OFF_T_VALUE % 10;

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return PGREST_ERROR;
        }

        if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
            return PGREST_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    return value;
}

time_t
pgrest_atotm(unsigned char *line, size_t n)
{
    time_t  value, cutoff, cutlim;

    if (n == 0) {
        return PGREST_ERROR;
    }

    cutoff = PGREST_MAX_TIME_T_VALUE / 10;
    cutlim = PGREST_MAX_TIME_T_VALUE % 10;

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return PGREST_ERROR;
        }

        if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
            return PGREST_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    return value;
}
