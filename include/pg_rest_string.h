/*-------------------------------------------------------------------------
 *
 * include/pg_rest_string.h
 *
 * string mgmt routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_STRING_H
#define PG_REST_STRING_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"

typedef struct pgrest_iovec_s    pgrest_iovec_t;
typedef pgrest_iovec_t           pgrest_string_t;

struct pgrest_iovec_s {
    unsigned char               *base;
    size_t                       len;
};

typedef struct {
    pgrest_string_t              key;
    pgrest_string_t              value;
} pgrest_param_t;

#define pgrest_string(str)     { (unsigned char *) str, sizeof(str) - 1 }
#define pgrest_string_set(str, text)                                            \
    (str)->len = sizeof(text) - 1; (str)->base = (unsigned char *) text
#define pgrest_string_null(str)   (str)->len = 0; (str)->base = NULL
#define pgrest_string_init(dst, src, size)                                      \
do {                                                                            \
    if ((dst)->base) {                                                          \
        pfree((dst)->base);                                                     \
    }                                                                           \
    (dst)->base = palloc((size) + 1);                                           \
    memcpy((dst)->base, (src), (size));                                         \
    (dst)->base[(size)] = '\0';                                                 \
    (dst)->len = (size);                                                        \
} while (0)

static inline int 
pgrest_tolower(int ch)
{
    return 'A' <= ch && ch <= 'Z' ? ch + 0x20 : ch;
}

static inline void 
pgrest_strtolower(char *s, size_t len)
{
    for (; len != 0; ++s, --len)
        *s = pgrest_tolower(*s);
}

int   pgrest_atoi(const char *arg, size_t len);
char *pgrest_strlchr(char *p, char *last, char c);
int   pgrest_count_params(pgrest_string_t path);
off_t pgrest_atoof(unsigned char *line, size_t n);
time_t pgrest_atotm(unsigned char *line, size_t n);

#endif /* PG_REST_STRING_H */
