/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_header.h
 *
 * http header routines.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_HEADER_H
#define PG_REST_HTTP_HEADER_H

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

#define PGREST_HEADER_MAX_TOKENS                        lengthof(pgrest_header_tokens)

#define PGREST_HEADER_TOKEN_AUTHORITY                   (pgrest_header_tokens + 0)
#define PGREST_HEADER_TOKEN_METHOD                      (pgrest_header_tokens + 1)
#define PGREST_HEADER_TOKEN_PATH                        (pgrest_header_tokens + 2)
#define PGREST_HEADER_TOKEN_SCHEME                      (pgrest_header_tokens + 3)
#define PGREST_HEADER_TOKEN_STATUS                      (pgrest_header_tokens + 4)
#define PGREST_HEADER_TOKEN_ACCEPT                      (pgrest_header_tokens + 5)
#define PGREST_HEADER_TOKEN_ACCEPT_CHARSET              (pgrest_header_tokens + 6)
#define PGREST_HEADER_TOKEN_ACCEPT_ENCODING             (pgrest_header_tokens + 7)
#define PGREST_HEADER_TOKEN_ACCEPT_LANGUAGE             (pgrest_header_tokens + 8)
#define PGREST_HEADER_TOKEN_ACCEPT_RANGES               (pgrest_header_tokens + 9)
#define PGREST_HEADER_TOKEN_ACCESS_CONTROL_ALLOW_ORIGIN (pgrest_header_tokens + 10)
#define PGREST_HEADER_TOKEN_AGE                         (pgrest_header_tokens + 11)
#define PGREST_HEADER_TOKEN_ALLOW                       (pgrest_header_tokens + 12)
#define PGREST_HEADER_TOKEN_AUTHORIZATION               (pgrest_header_tokens + 13)
#define PGREST_HEADER_TOKEN_CACHE_CONTROL               (pgrest_header_tokens + 14)
#define PGREST_HEADER_TOKEN_CACHE_DIGEST                (pgrest_header_tokens + 15)
#define PGREST_HEADER_TOKEN_CONNECTION                  (pgrest_header_tokens + 16)
#define PGREST_HEADER_TOKEN_CONTENT_DISPOSITION         (pgrest_header_tokens + 17)
#define PGREST_HEADER_TOKEN_CONTENT_ENCODING            (pgrest_header_tokens + 18)
#define PGREST_HEADER_TOKEN_CONTENT_LANGUAGE            (pgrest_header_tokens + 19)
#define PGREST_HEADER_TOKEN_CONTENT_LENGTH              (pgrest_header_tokens + 20)
#define PGREST_HEADER_TOKEN_CONTENT_LOCATION            (pgrest_header_tokens + 21)
#define PGREST_HEADER_TOKEN_CONTENT_RANGE               (pgrest_header_tokens + 22)
#define PGREST_HEADER_TOKEN_CONTENT_TYPE                (pgrest_header_tokens + 23)
#define PGREST_HEADER_TOKEN_COOKIE                      (pgrest_header_tokens + 24)
#define PGREST_HEADER_TOKEN_DATE                        (pgrest_header_tokens + 25)
#define PGREST_HEADER_TOKEN_ETAG                        (pgrest_header_tokens + 26)
#define PGREST_HEADER_TOKEN_EXPECT                      (pgrest_header_tokens + 27)
#define PGREST_HEADER_TOKEN_EXPIRES                     (pgrest_header_tokens + 28)
#define PGREST_HEADER_TOKEN_FROM                        (pgrest_header_tokens + 29)
#define PGREST_HEADER_TOKEN_HOST                        (pgrest_header_tokens + 30)
#define PGREST_HEADER_TOKEN_HTTP2_SETTINGS              (pgrest_header_tokens + 31)
#define PGREST_HEADER_TOKEN_IF_MATCH                    (pgrest_header_tokens + 32)
#define PGREST_HEADER_TOKEN_IF_MODIFIED_SINCE           (pgrest_header_tokens + 33)
#define PGREST_HEADER_TOKEN_IF_NONE_MATCH               (pgrest_header_tokens + 34)
#define PGREST_HEADER_TOKEN_IF_RANGE                    (pgrest_header_tokens + 35)
#define PGREST_HEADER_TOKEN_IF_UNMODIFIED_SINCE         (pgrest_header_tokens + 36)
#define PGREST_HEADER_TOKEN_KEEP_ALIVE                  (pgrest_header_tokens + 37)
#define PGREST_HEADER_TOKEN_LAST_MODIFIED               (pgrest_header_tokens + 38)
#define PGREST_HEADER_TOKEN_LINK                        (pgrest_header_tokens + 39)
#define PGREST_HEADER_TOKEN_LOCATION                    (pgrest_header_tokens + 40)
#define PGREST_HEADER_TOKEN_MAX_FORWARDS                (pgrest_header_tokens + 41)
#define PGREST_HEADER_TOKEN_PROXY_AUTHENTICATE          (pgrest_header_tokens + 42)
#define PGREST_HEADER_TOKEN_PROXY_AUTHORIZATION         (pgrest_header_tokens + 43)
#define PGREST_HEADER_TOKEN_RANGE                       (pgrest_header_tokens + 44)
#define PGREST_HEADER_TOKEN_REFERER                     (pgrest_header_tokens + 45)
#define PGREST_HEADER_TOKEN_REFRESH                     (pgrest_header_tokens + 46)
#define PGREST_HEADER_TOKEN_RETRY_AFTER                 (pgrest_header_tokens + 47)
#define PGREST_HEADER_TOKEN_SERVER                      (pgrest_header_tokens + 48)
#define PGREST_HEADER_TOKEN_SET_COOKIE                  (pgrest_header_tokens + 49)
#define PGREST_HEADER_TOKEN_STRICT_TRANSPORT_SECURITY   (pgrest_header_tokens + 50)
#define PGREST_HEADER_TOKEN_TE                          (pgrest_header_tokens + 51)
#define PGREST_HEADER_TOKEN_TRANSFER_ENCODING           (pgrest_header_tokens + 52)
#define PGREST_HEADER_TOKEN_UPGRADE                     (pgrest_header_tokens + 53)
#define PGREST_HEADER_TOKEN_USER_AGENT                  (pgrest_header_tokens + 54)
#define PGREST_HEADER_TOKEN_VARY                        (pgrest_header_tokens + 55)
#define PGREST_HEADER_TOKEN_VIA                         (pgrest_header_tokens + 56)
#define PGREST_HEADER_TOKEN_WWW_AUTHENTICATE            (pgrest_header_tokens + 57)
#define PGREST_HEADER_TOKEN_X_COMPRESS_HINT             (pgrest_header_tokens + 58)
#define PGREST_HEADER_TOKEN_X_FORWARDED_FOR             (pgrest_header_tokens + 59)
#define PGREST_HEADER_TOKEN_X_REPROXY_URL               (pgrest_header_tokens + 60)
#define PGREST_HEADER_TOKEN_X_TRAFFIC                   (pgrest_header_tokens + 61)

typedef struct {
    pgrest_string_t                   elem;
    pgrest_http_header_handler_pt     handler;
    size_t                            offset;
    char                              index;
    unsigned                          drop:1;
    unsigned                          reject:1;
    unsigned                          copy:1;
} pgrest_http_header_token_t;

const pgrest_http_header_token_t *
pgrest_http_header_find_token(const char *name, size_t len);
bool pgrest_http_header_is_token(const pgrest_string_t *str);
bool pgrest_http_header_add(pgrest_array_t *headers, pgrest_param_t **header);
void
pgrest_http_header_align(pgrest_http_headers_in_t *headers_in, 
                         unsigned char *old,
                         unsigned char *new);

#endif /* PG_REST_HTTP_HEADER_H */
