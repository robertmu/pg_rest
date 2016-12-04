/* -------------------------------------------------------------------------
 *
 * pq_rest_http_header.c
 *   http header routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_http_header.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"
#include "pg_rest_http.h"

static pgrest_http_header_token_t pgrest_header_tokens[] = {
    {pgrest_string(":authority"), NULL, 0, 1, 0, 0, 0},
    {pgrest_string(":method"), NULL, 0, 2, 0, 0, 0},
    {pgrest_string(":path"), NULL, 0, 4, 0, 0, 0},
    {pgrest_string(":scheme"), NULL, 0, 6, 0, 0, 0},
    {pgrest_string(":status"), NULL, 0, 8, 0, 0, 0},
    {pgrest_string("accept"), NULL, 0, 19, 0, 0, 1},
    {pgrest_string("accept-charset"), NULL, 0, 15, 0, 0, 1},
    {pgrest_string("accept-encoding"), NULL, 0, 16, 0, 0, 1},
    {pgrest_string("accept-language"), NULL, 0, 17, 0, 0, 1},
    {pgrest_string("accept-ranges"), NULL, 0, 18, 0, 0, 0},
    {pgrest_string("access-control-allow-origin"), NULL, 0, 20, 0, 0, 0},
    {pgrest_string("age"), NULL, 0, 21, 0, 0, 0},
    {pgrest_string("allow"), NULL, 0, 22, 0, 0, 0},
    {pgrest_string("authorization"), NULL, 0, 23, 0, 0, 0},
    {pgrest_string("cache-control"), NULL, 0, 24, 0, 0, 0},
    {pgrest_string("cache-digest"), NULL, 0, 0, 0, 0, 0},
    {pgrest_string("connection"), NULL, 0, 0, 1, 1, 0},
    {pgrest_string("content-disposition"), NULL, 0, 25, 0, 0, 0},
    {pgrest_string("content-encoding"), NULL, 0, 26, 0, 0, 0},
    {pgrest_string("content-language"), NULL, 0, 27, 0, 0, 0},
    {pgrest_string("content-length"), NULL, 0, 28, 0, 0, 0},
    {pgrest_string("content-location"), NULL, 0, 29, 0, 0, 0},
    {pgrest_string("content-range"), NULL, 0, 30, 0, 0, 0},
    {pgrest_string("content-type"), NULL, 0, 31, 0, 0, 0},
    {pgrest_string("cookie"), NULL, 0, 32, 0, 0, 0},
    {pgrest_string("date"), NULL, 0, 33, 1, 0, 0},
    {pgrest_string("etag"), NULL, 0, 34, 0, 0, 0},
    {pgrest_string("expect"), NULL, 0, 35, 0, 0, 0},
    {pgrest_string("expires"), NULL, 0, 36, 0, 0, 0},
    {pgrest_string("from"), NULL, 0, 37, 0, 0, 0},
    {pgrest_string("host"), NULL, 0, 38, 0, 1, 0},
    {pgrest_string("http2-settings"), NULL, 0, 0, 1, 1, 0},
    {pgrest_string("if-match"), NULL, 0, 39, 0, 0, 0},
    {pgrest_string("if-modified-since"), NULL, 0, 40, 0, 0, 0},
    {pgrest_string("if-none-match"), NULL, 0, 41, 0, 0, 0},
    {pgrest_string("if-range"), NULL, 0, 42, 0, 0, 0},
    {pgrest_string("if-unmodified-since"), NULL, 0, 43, 0, 0, 0},
    {pgrest_string("keep-alive"), NULL, 0, 0, 1, 0, 0},
    {pgrest_string("last-modified"), NULL, 0, 44, 0, 0, 0},
    {pgrest_string("link"), NULL, 0, 45, 0, 0, 0},
    {pgrest_string("location"), NULL, 0, 46, 0, 0, 0},
    {pgrest_string("max-forwards"), NULL, 0, 47, 0, 0, 0},
    {pgrest_string("proxy-authenticate"), NULL, 0, 48, 1, 0, 0},
    {pgrest_string("proxy-authorization"), NULL, 0, 49, 1, 0, 0},
    {pgrest_string("range"), NULL, 0, 50, 0, 0, 0},
    {pgrest_string("referer"), NULL, 0, 51, 0, 0, 0},
    {pgrest_string("refresh"), NULL, 0, 52, 0, 0, 0},
    {pgrest_string("retry-after"), NULL, 0, 53, 0, 0, 0},
    {pgrest_string("server"), NULL, 0, 54, 1, 0, 0},
    {pgrest_string("set-cookie"), NULL, 0, 55, 0, 0, 0},
    {pgrest_string("strict-transport-security"), NULL, 0, 56, 0, 0, 0},
    {pgrest_string("te"), NULL, 0, 0, 1, 1, 0},
    {pgrest_string("transfer-encoding"), NULL, 0, 57, 1, 1, 0},
    {pgrest_string("upgrade"), NULL, 0, 0, 1, 1, 0},
    {pgrest_string("user-agent"), NULL, 0, 58, 0, 0, 1},
    {pgrest_string("vary"), NULL, 0, 59, 0, 0, 0},
    {pgrest_string("via"), NULL, 0, 60, 0, 0, 0},
    {pgrest_string("www-authenticate"), NULL, 0, 61, 0, 0, 0},
    {pgrest_string("x-compress-hint"), NULL, 0, 0, 0, 0, 0},
    {pgrest_string("x-forwarded-for"), NULL, 0, 0, 0, 0, 0},
    {pgrest_string("x-reproxy-url"), NULL, 0, 0, 0, 0, 0},
    {pgrest_string("x-traffic"), NULL, 0, 0, 0, 0, 0}
};

bool 
pgrest_http_header_is_token(const pgrest_string_t *str)
{
    return &pgrest_header_tokens[0].elem <= str && 
            str <= &pgrest_header_tokens[PGREST_HEADER_MAX_TOKENS - 1].elem;
}

const pgrest_http_header_token_t *
pgrest_http_header_find_token(const char *name, size_t len)
{
    switch (len) {
    case 2:
        switch (name[1]) {
        case 'e':
            if (memcmp(name, "t", 1) == 0)
                return PGREST_HEADER_TOKEN_TE;
            break;
        }
        break;
    case 3:
        switch (name[2]) {
        case 'a':
            if (memcmp(name, "vi", 2) == 0)
                return PGREST_HEADER_TOKEN_VIA;
            break;
        case 'e':
            if (memcmp(name, "ag", 2) == 0)
                return PGREST_HEADER_TOKEN_AGE;
            break;
        }
        break;
    case 4:
        switch (name[3]) {
        case 'e':
            if (memcmp(name, "dat", 3) == 0)
                return PGREST_HEADER_TOKEN_DATE;
            break;
        case 'g':
            if (memcmp(name, "eta", 3) == 0)
                return PGREST_HEADER_TOKEN_ETAG;
            break;
        case 'k':
            if (memcmp(name, "lin", 3) == 0)
                return PGREST_HEADER_TOKEN_LINK;
            break;
        case 'm':
            if (memcmp(name, "fro", 3) == 0)
                return PGREST_HEADER_TOKEN_FROM;
            break;
        case 't':
            if (memcmp(name, "hos", 3) == 0)
                return PGREST_HEADER_TOKEN_HOST;
            break;
        case 'y':
            if (memcmp(name, "var", 3) == 0)
                return PGREST_HEADER_TOKEN_VARY;
            break;
        }
        break;
    case 5:
        switch (name[4]) {
        case 'e':
            if (memcmp(name, "rang", 4) == 0)
                return PGREST_HEADER_TOKEN_RANGE;
            break;
        case 'h':
            if (memcmp(name, ":pat", 4) == 0)
                return PGREST_HEADER_TOKEN_PATH;
            break;
        case 'w':
            if (memcmp(name, "allo", 4) == 0)
                return PGREST_HEADER_TOKEN_ALLOW;
            break;
        }
        break;
    case 6:
        switch (name[5]) {
        case 'e':
            if (memcmp(name, "cooki", 5) == 0)
                return PGREST_HEADER_TOKEN_COOKIE;
            break;
        case 'r':
            if (memcmp(name, "serve", 5) == 0)
                return PGREST_HEADER_TOKEN_SERVER;
            break;
        case 't':
            if (memcmp(name, "accep", 5) == 0)
                return PGREST_HEADER_TOKEN_ACCEPT;
            if (memcmp(name, "expec", 5) == 0)
                return PGREST_HEADER_TOKEN_EXPECT;
            break;
        }
        break;
    case 7:
        switch (name[6]) {
        case 'd':
            if (memcmp(name, ":metho", 6) == 0)
                return PGREST_HEADER_TOKEN_METHOD;
            break;
        case 'e':
            if (memcmp(name, ":schem", 6) == 0)
                return PGREST_HEADER_TOKEN_SCHEME;
            if (memcmp(name, "upgrad", 6) == 0)
                return PGREST_HEADER_TOKEN_UPGRADE;
            break;
        case 'h':
            if (memcmp(name, "refres", 6) == 0)
                return PGREST_HEADER_TOKEN_REFRESH;
            break;
        case 'r':
            if (memcmp(name, "refere", 6) == 0)
                return PGREST_HEADER_TOKEN_REFERER;
            break;
        case 's':
            if (memcmp(name, ":statu", 6) == 0)
                return PGREST_HEADER_TOKEN_STATUS;
            if (memcmp(name, "expire", 6) == 0)
                return PGREST_HEADER_TOKEN_EXPIRES;
            break;
        }
        break;
    case 8:
        switch (name[7]) {
        case 'e':
            if (memcmp(name, "if-rang", 7) == 0)
                return PGREST_HEADER_TOKEN_IF_RANGE;
            break;
        case 'h':
            if (memcmp(name, "if-matc", 7) == 0)
                return PGREST_HEADER_TOKEN_IF_MATCH;
            break;
        case 'n':
            if (memcmp(name, "locatio", 7) == 0)
                return PGREST_HEADER_TOKEN_LOCATION;
            break;
        }
        break;
    case 9:
        switch (name[8]) {
        case 'c':
            if (memcmp(name, "x-traffi", 8) == 0)
                return PGREST_HEADER_TOKEN_X_TRAFFIC;
            break;
        }
        break;
    case 10:
        switch (name[9]) {
        case 'e':
            if (memcmp(name, "keep-aliv", 9) == 0)
                return PGREST_HEADER_TOKEN_KEEP_ALIVE;
            if (memcmp(name, "set-cooki", 9) == 0)
                return PGREST_HEADER_TOKEN_SET_COOKIE;
            break;
        case 'n':
            if (memcmp(name, "connectio", 9) == 0)
                return PGREST_HEADER_TOKEN_CONNECTION;
            break;
        case 't':
            if (memcmp(name, "user-agen", 9) == 0)
                return PGREST_HEADER_TOKEN_USER_AGENT;
            break;
        case 'y':
            if (memcmp(name, ":authorit", 9) == 0)
                return PGREST_HEADER_TOKEN_AUTHORITY;
            break;
        }
        break;
    case 11:
        switch (name[10]) {
        case 'r':
            if (memcmp(name, "retry-afte", 10) == 0)
                return PGREST_HEADER_TOKEN_RETRY_AFTER;
            break;
        }
        break;
    case 12:
        switch (name[11]) {
        case 'e':
            if (memcmp(name, "content-typ", 11) == 0)
                return PGREST_HEADER_TOKEN_CONTENT_TYPE;
            break;
        case 's':
            if (memcmp(name, "max-forward", 11) == 0)
                return PGREST_HEADER_TOKEN_MAX_FORWARDS;
            break;
        case 't':
            if (memcmp(name, "cache-diges", 11) == 0)
                return PGREST_HEADER_TOKEN_CACHE_DIGEST;
            break;
        }
        break;
    case 13:
        switch (name[12]) {
        case 'd':
            if (memcmp(name, "last-modifie", 12) == 0)
                return PGREST_HEADER_TOKEN_LAST_MODIFIED;
            break;
        case 'e':
            if (memcmp(name, "content-rang", 12) == 0)
                return PGREST_HEADER_TOKEN_CONTENT_RANGE;
            break;
        case 'h':
            if (memcmp(name, "if-none-matc", 12) == 0)
                return PGREST_HEADER_TOKEN_IF_NONE_MATCH;
            break;
        case 'l':
            if (memcmp(name, "cache-contro", 12) == 0)
                return PGREST_HEADER_TOKEN_CACHE_CONTROL;
            if (memcmp(name, "x-reproxy-ur", 12) == 0)
                return PGREST_HEADER_TOKEN_X_REPROXY_URL;
            break;
        case 'n':
            if (memcmp(name, "authorizatio", 12) == 0)
                return PGREST_HEADER_TOKEN_AUTHORIZATION;
            break;
        case 's':
            if (memcmp(name, "accept-range", 12) == 0)
                return PGREST_HEADER_TOKEN_ACCEPT_RANGES;
            break;
        }
        break;
    case 14:
        switch (name[13]) {
        case 'h':
            if (memcmp(name, "content-lengt", 13) == 0)
                return PGREST_HEADER_TOKEN_CONTENT_LENGTH;
            break;
        case 's':
            if (memcmp(name, "http2-setting", 13) == 0)
                return PGREST_HEADER_TOKEN_HTTP2_SETTINGS;
            break;
        case 't':
            if (memcmp(name, "accept-charse", 13) == 0)
                return PGREST_HEADER_TOKEN_ACCEPT_CHARSET;
            break;
        }
        break;
    case 15:
        switch (name[14]) {
        case 'e':
            if (memcmp(name, "accept-languag", 14) == 0)
                return PGREST_HEADER_TOKEN_ACCEPT_LANGUAGE;
            break;
        case 'g':
            if (memcmp(name, "accept-encodin", 14) == 0)
                return PGREST_HEADER_TOKEN_ACCEPT_ENCODING;
            break;
        case 'r':
            if (memcmp(name, "x-forwarded-fo", 14) == 0)
                return PGREST_HEADER_TOKEN_X_FORWARDED_FOR;
            break;
        case 't':
            if (memcmp(name, "x-compress-hin", 14) == 0)
                return PGREST_HEADER_TOKEN_X_COMPRESS_HINT;
            break;
        }
        break;
    case 16:
        switch (name[15]) {
        case 'e':
            if (memcmp(name, "content-languag", 15) == 0)
                return PGREST_HEADER_TOKEN_CONTENT_LANGUAGE;
            if (memcmp(name, "www-authenticat", 15) == 0)
                return PGREST_HEADER_TOKEN_WWW_AUTHENTICATE;
            break;
        case 'g':
            if (memcmp(name, "content-encodin", 15) == 0)
                return PGREST_HEADER_TOKEN_CONTENT_ENCODING;
            break;
        case 'n':
            if (memcmp(name, "content-locatio", 15) == 0)
                return PGREST_HEADER_TOKEN_CONTENT_LOCATION;
            break;
        }
        break;
    case 17:
        switch (name[16]) {
        case 'e':
            if (memcmp(name, "if-modified-sinc", 16) == 0)
                return PGREST_HEADER_TOKEN_IF_MODIFIED_SINCE;
            break;
        case 'g':
            if (memcmp(name, "transfer-encodin", 16) == 0)
                return PGREST_HEADER_TOKEN_TRANSFER_ENCODING;
            break;
        }
        break;
    case 18:
        switch (name[17]) {
        case 'e':
            if (memcmp(name, "proxy-authenticat", 17) == 0)
                return PGREST_HEADER_TOKEN_PROXY_AUTHENTICATE;
            break;
        }
        break;
    case 19:
        switch (name[18]) {
        case 'e':
            if (memcmp(name, "if-unmodified-sinc", 18) == 0)
                return PGREST_HEADER_TOKEN_IF_UNMODIFIED_SINCE;
            break;
        case 'n':
            if (memcmp(name, "content-dispositio", 18) == 0)
                return PGREST_HEADER_TOKEN_CONTENT_DISPOSITION;
            if (memcmp(name, "proxy-authorizatio", 18) == 0)
                return PGREST_HEADER_TOKEN_PROXY_AUTHORIZATION;
            break;
        }
        break;
    case 25:
        switch (name[24]) {
        case 'y':
            if (memcmp(name, "strict-transport-securit", 24) == 0)
                return PGREST_HEADER_TOKEN_STRICT_TRANSPORT_SECURITY;
            break;
        }
        break;
    case 27:
        switch (name[26]) {
        case 'n':
            if (memcmp(name, "access-control-allow-origi", 26) == 0)
                return PGREST_HEADER_TOKEN_ACCESS_CONTROL_ALLOW_ORIGIN;
            break;
        }
        break;
    }

    return NULL;
}

bool
pgrest_http_header_add(pgrest_array_t *headers, pgrest_param_t **header)
{
    pgrest_param_t *elem;

    elem = pgrest_array_push(headers);
    if (elem == NULL) {
        return false;
    }

   *elem = **header;
   *header = elem;

    return true;
}

void
pgrest_http_header_align(pgrest_http_headers_in_t *headers_in, 
                         unsigned char *old,
                         unsigned char *new)
{
    pgrest_uint_t   i;
    pgrest_param_t *elem;
    pgrest_array_t *headers = &headers_in->headers;

    elem = headers->elts;
    for (i = 0; i < headers->size; i++) {
        elem[i].key.base = new + (elem[i].key.base - old);
        elem[i].value.base = new + (elem[i].value.base - old);
    }

    if (headers_in->user.base) {
        headers_in->user.base = new + (headers_in->user.base - old);
    }

    if (headers_in->passwd.base) {
        headers_in->passwd.base = new + (headers_in->passwd.base - old);
    }

    if (headers_in->server.base) {
        headers_in->server.base = new + (headers_in->server.base - old);
    }
}
