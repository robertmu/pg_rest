/* -------------------------------------------------------------------------
 *
 * pq_rest_inet.c
 *   inet routines
 *
 *
 *
 * Copyright (C) 2014-2015, Robert Mu <dbx_c@hotmail.com>
 *
 *  src/pg_rest_inet.c
 *
 * -------------------------------------------------------------------------
 */

#include "pg_rest_config.h"
#include "pg_rest_core.h"

const char *
pgrest_inet_ntop(struct sockaddr *sa, socklen_t socklen, 
                 char *dst, size_t size, bool port)
{
    struct sockaddr_in   *sin;
    size_t                len;
#ifdef HAVE_IPV6
    struct sockaddr_in6  *sin6;
#endif

    switch (sa->sa_family) {
    case AF_INET:
        sin = (struct sockaddr_in *) sa;

        if(!evutil_inet_ntop(AF_INET, &sin->sin_addr.s_addr, dst, size)) {
            return NULL;
        }

        if (port) {
            len = strlen(dst);
            snprintf(dst + len, size - len, ":%d", ntohs(sin->sin_port));
        } 

        return dst;
#ifdef HAVE_IPV6
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) sa;

        if(!evutil_inet_ntop(AF_INET6, &sin6->sin6_addr.s6_addr, dst, size)) {
            return NULL;
        }

        if (port) {
            len = strlen(dst);
            dst[len++] = '[';
            snprintf(dst + len, size - len, "]:%d", ntohs(sin6->sin6_port));
        } 

        return dst;
#endif
#ifdef HAVE_UNIX_SOCKETS
    case AF_UNIX:
        if (socklen <= (socklen_t) offsetof(struct sockaddr_un, sun_path)) {
            snprintf(dst, size, "????");
        } else {
            snprintf(dst, size, "%s", ((struct sockaddr_un *) sa)->sun_path);
        }

        return dst;
#endif
    default:
        ereport(WARNING,
                (errmsg(PGREST_PACKAGE " " "unrecognized address family %d", 
                         sa->sa_family)));
        return NULL;
    }
}

bool
pgrest_inet_cmp_sockaddr(struct sockaddr *sa1, socklen_t slen1,
    struct sockaddr *sa2, socklen_t slen2, bool cmp_port)
{
    struct sockaddr_in   *sin1, *sin2;
#ifdef HAVE_IPV6
    struct sockaddr_in6  *sin61, *sin62;
#endif
#ifdef HAVE_UNIX_SOCKETS
    struct sockaddr_un   *saun1, *saun2;
#endif

    if (sa1->sa_family != sa2->sa_family) {
        return false;
    }

    switch (sa1->sa_family) {

#ifdef HAVE_IPV6
    case AF_INET6:

        sin61 = (struct sockaddr_in6 *) sa1;
        sin62 = (struct sockaddr_in6 *) sa2;

        if (cmp_port && sin61->sin6_port != sin62->sin6_port) {
            return false;
        }

        if (memcmp(&sin61->sin6_addr, &sin62->sin6_addr, 16) != 0) {
            return false;
        }

        break;
#endif

#ifdef HAVE_UNIX_SOCKETS
    case AF_UNIX:

        saun1 = (struct sockaddr_un *) sa1;
        saun2 = (struct sockaddr_un *) sa2;

        if (memcmp(&saun1->sun_path, &saun2->sun_path,
                       sizeof(saun1->sun_path))
            != 0)
        {
            return false;
        }

        break;
#endif

    default: /* AF_INET */

        sin1 = (struct sockaddr_in *) sa1;
        sin2 = (struct sockaddr_in *) sa2;

        if (cmp_port && sin1->sin_port != sin2->sin_port) {
            return false;
        }

        if (sin1->sin_addr.s_addr != sin2->sin_addr.s_addr) {
            return false;
        }

        break;
    }

    return true;
}

in_port_t
pgrest_inet_get_port(struct sockaddr *sa)
{
    struct sockaddr_in   *sin;
#ifdef HAVE_IPV6
    struct sockaddr_in6  *sin6;
#endif

    switch (sa->sa_family) {

#ifdef HAVE_IPV6
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) sa;
        return ntohs(sin6->sin6_port);
#endif

#ifdef HAVE_UNIX_SOCKETS
    case AF_UNIX:
        return 0;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) sa;
        return ntohs(sin->sin_port);
    }
}

void
pgrest_inet_set_port(struct sockaddr *sa, in_port_t port)
{
    struct sockaddr_in   *sin;
#ifdef HAVE_IPV6
    struct sockaddr_in6  *sin6;
#endif

    switch (sa->sa_family) {

#ifdef HAVE_IPV6
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) sa;
        sin6->sin6_port = htons(port);
        break;
#endif

#ifdef HAVE_UNIX_SOCKETS
    case AF_UNIX:
        break;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) sa;
        sin->sin_port = htons(port);
        break;
    }
}

in_addr_t
pgrest_inet_inet_addr(unsigned char *text, size_t len)
{
    unsigned char  *p, c;
    in_addr_t       addr;
    pgrest_uint_t   octet, n;

    addr = 0;
    octet = 0;
    n = 0;

    for (p = text; p < text + len; p++) {
        c = *p;

        if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');

            if (octet > 255) {
                return INADDR_NONE;
            }

            continue;
        }

        if (c == '.') {
            addr = (addr << 8) + octet;
            octet = 0;
            n++;
            continue;
        }

        return INADDR_NONE;
    }

    if (n == 3) {
        addr = (addr << 8) + octet;
        return htonl(addr);
    }

    return INADDR_NONE;
}


#ifdef HAVE_IPV6
bool
pgrest_inet_inet6_addr(unsigned char *p, size_t len, unsigned char *addr)
{
    unsigned char  c, *zero, *digit, *s, *d;
    size_t         len4;
    pgrest_uint_t  n, nibbles, word;

    if (len == 0) {
        return false;
    }

    zero = NULL;
    digit = NULL;
    len4 = 0;
    nibbles = 0;
    word = 0;
    n = 8;

    if (p[0] == ':') {
        p++;
        len--;
    }

    for (/* void */; len; len--) {
        c = *p++;

        if (c == ':') {
            if (nibbles) {
                digit = p;
                len4 = len;
                *addr++ = (unsigned char) (word >> 8);
                *addr++ = (unsigned char) (word & 0xff);

                if (--n) {
                    nibbles = 0;
                    word = 0;
                    continue;
                }

            } else {
                if (zero == NULL) {
                    digit = p;
                    len4 = len;
                    zero = addr;
                    continue;
                }
            }

            return false;
        }

        if (c == '.' && nibbles) {
            if (n < 2 || digit == NULL) {
                return false;
            }

            word = pgrest_inet_inet_addr(digit, len4 - 1);
            if (word == INADDR_NONE) {
                return false;
            }

            word = ntohl(word);
            *addr++ = (unsigned char) ((word >> 24) & 0xff);
            *addr++ = (unsigned char) ((word >> 16) & 0xff);
            n--;
            break;
        }

        if (++nibbles > 4) {
            return false;
        }

        if (c >= '0' && c <= '9') {
            word = word * 16 + (c - '0');
            continue;
        }

        c |= 0x20;

        if (c >= 'a' && c <= 'f') {
            word = word * 16 + (c - 'a') + 10;
            continue;
        }

        return false;
    }

    if (nibbles == 0 && zero == NULL) {
        return false;
    }

    *addr++ = (unsigned char) (word >> 8);
    *addr++ = (unsigned char) (word & 0xff);

    if (--n) {
        if (zero) {
            n *= 2;
            s = addr - 1;
            d = s + n;
            while (s >= zero) {
                *d-- = *s--;
            }
            MemSet(zero, 0, n);
            return true;
        }

    } else {
        if (zero == NULL) {
            return true;
        }
    }

    return false;
}
#endif

#if defined(HAVE_IPV6) && defined(HAVE_GETADDRINFO)
bool
pgrest_inet_resolve_host(pgrest_url_t *u)
{
    char                 *p, *host;
    size_t                len;
    in_port_t             port;
    pgrest_uint_t         i;
    struct addrinfo       hints, *res, *rp;
    struct sockaddr_in   *sin;
    struct sockaddr_in6  *sin6;

    port = htons(u->port);

    host = palloc(u->host_len + 1);
    StrNCpy(host, u->host, u->host_len + 1);

    MemSet(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
#ifdef AI_ADDRCONFIG
    hints.ai_flags = AI_ADDRCONFIG;
#endif

    if (getaddrinfo((char *) host, NULL, &hints, &res) != 0) {
        u->err = "host not found";
        pfree(host);
        return false;
    }

    pfree(host);

    for (i = 0, rp = res; rp != NULL; rp = rp->ai_next) {

        switch (rp->ai_family) {

        case AF_INET:
        case AF_INET6:
            break;

        default:
            continue;
        }

        i++;
    }

    if (i == 0) {
        u->err = "host not found";
        goto failed;
    }

    u->addrs = palloc0(i * sizeof(pgrest_addr_t));
    u->naddrs = i;

    i = 0;

    /* AF_INET addresses first */
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET) {
            continue;
        }

        sin = palloc0(rp->ai_addrlen);

        memcpy(sin, rp->ai_addr, rp->ai_addrlen);

        sin->sin_port = port;

        u->addrs[i].sockaddr = (struct sockaddr *) sin;
        u->addrs[i].socklen = rp->ai_addrlen;

        len = PGREST_INET_ADDRSTRLEN + sizeof(":65535") - 1;
        p = palloc(len);

        (void) pgrest_inet_ntop((struct sockaddr *) sin, 
                                     rp->ai_addrlen, 
                                     p, 
                                     len, 
                                     true);

        u->addrs[i].name_len = strlen(p);
        u->addrs[i].name = p;

        i++;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {

        if (rp->ai_family != AF_INET6) {
            continue;
        }

        sin6 = palloc0(rp->ai_addrlen);
        memcpy(sin6, rp->ai_addr, rp->ai_addrlen);

        sin6->sin6_port = port;

        u->addrs[i].sockaddr = (struct sockaddr *) sin6;
        u->addrs[i].socklen = rp->ai_addrlen;

        len = PGREST_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1;
        p = palloc(len);

        (void) pgrest_inet_ntop((struct sockaddr *) sin6, 
                                     rp->ai_addrlen, 
                                     p,
                                     len, 
                                     true);

        u->addrs[i].name_len = strlen(p);
        u->addrs[i].name = p;

        i++;
    }

    freeaddrinfo(res);
    return true;

failed:

    freeaddrinfo(res);
    return false;
}

#else

bool
pgrest_inet_resolve_host(pgrest_url_t *u)
{
    char                *p, *host;
    size_t               len;
    in_port_t            port;
    in_addr_t            in_addr;
    pgrest_uint_t        i;
    struct hostent      *h;
    struct sockaddr_in  *sin;

    /* AF_INET only */
    port = htons(u->port);

    in_addr = pgrest_util_inet_addr(u->host, u->host_len);

    if (in_addr == INADDR_NONE) {
        host = palloc(u->host_len + 1);
        StrNCpy(host, u->host, u->host_len + 1);

        h = gethostbyname((char *) host);

        pfree(host);

        if (h == NULL || h->h_addr_list[0] == NULL) {
            u->err = "host not found";
            return false;
        }

        for (i = 0; h->h_addr_list[i] != NULL; i++) { /* void */ }

        u->addrs = palloc0(i * sizeof(pgrest_addr_t));
        u->naddrs = i;

        for (i = 0; i < u->naddrs; i++) {
            sin = palloc0(sizeof(struct sockaddr_in));

            sin->sin_family = AF_INET;
            sin->sin_port = port;
            sin->sin_addr.s_addr = *(in_addr_t *) (h->h_addr_list[i]);

            u->addrs[i].sockaddr = (struct sockaddr *) sin;
            u->addrs[i].socklen = sizeof(struct sockaddr_in);

            len = PGREST_INET_ADDRSTRLEN + sizeof(":65535") - 1;
            p = palloc(len);

            (void) pgrest_util_inet_ntop((struct sockaddr *) sin,
                                         sizeof(struct sockaddr_in), 
                                         p, 
                                         len, 
                                         true);

            u->addrs[i].name_len = strlen(p);
            u->addrs[i].name = p;
        }

    } else {
        u->addrs = palloc0(sizeof(pgrest_addr_t));
        sin = palloc0(sizeof(struct sockaddr_in));
        u->naddrs = 1;

        sin->sin_family = AF_INET;
        sin->sin_port = port;
        sin->sin_addr.s_addr = in_addr;

        u->addrs[0].sockaddr = (struct sockaddr *) sin;
        u->addrs[0].socklen = sizeof(struct sockaddr_in);

        p = palloc(u->host_len + sizeof(":65535"));

        memcpy(p, u->host, u->host_len);
        len = sprintf(p + u->host_len, ":%d", ntohs(port));
        u->addrs[0].name_len = u->host_len + len;
        u->addrs[0].name = p;
    }

    return true;
}
#endif /* HAVE_GETADDRINFO && HAVE_IPV6 */

static bool
pgrest_inet_parse_unix(pgrest_url_t *u)
{
#ifdef HAVE_UNIX_SOCKETS
    char                *path;
    size_t               len;
    struct sockaddr_un  *saun;

    len = u->url_len;
    path = u->url;

    path += 5;
    len -= 5;

    if (len == 0) {
        u->err = "no path in the unix domain socket";
        return false;
    }

    u->host_len = len++;
    u->host = path;

    if (len > sizeof(saun->sun_path)) {
        u->err = "too long path in the unix domain socket";
        return false;
    }

    u->socklen = sizeof(struct sockaddr_un);
    saun = (struct sockaddr_un *) &u->sockaddr;
    saun->sun_family = AF_UNIX;

    StrNCpy((char *) saun->sun_path, path, len);

    u->addrs = palloc0(sizeof(pgrest_addr_t));
    saun = palloc0(sizeof(struct sockaddr_un));

    u->family = AF_UNIX;
    u->naddrs = 1;

    saun->sun_family = AF_UNIX;
    StrNCpy((char *) saun->sun_path, path, len);

    u->addrs[0].sockaddr = (struct sockaddr *) saun;
    u->addrs[0].socklen = sizeof(struct sockaddr_un);
    u->addrs[0].name_len = len + 4;
    u->addrs[0].name = u->url;

    return true;
#else
    u->err = "the unix domain sockets are not supported on this platform";
    return false;
#endif
}

static bool
pgrest_inet_parse_inet6(pgrest_url_t *u)
{
#ifdef HAVE_IPV6
    char                 *p, *host, *port, *last;
    size_t                len;
    int                   n;
    struct sockaddr_in6  *sin6;

    u->socklen = sizeof(struct sockaddr_in6);
    sin6 = (struct sockaddr_in6 *) &u->sockaddr;
    sin6->sin6_family = AF_INET6;

    host = u->url + 1;

    last = u->url + u->url_len;

    p = pgrest_strlchr(host, last, ']');

    if (p == NULL) {
        u->err = "invalid host";
        return false;
    }

    port = p + 1;

    if (port < last) {
        if (*port != ':') {
            u->err = "invalid host";
            return false;
        }

        port++;

        len = last - port;

        n = pgrest_atoi(port, len);

        if (n < 1 || n > 65535) {
            u->err = "invalid port";
            return false;
        }

        u->port = (in_port_t) n;
        sin6->sin6_port = htons((in_port_t) n);

        u->port_text_len = len;
        u->port_text = port;

    } else {
        u->no_port = 1;
        u->port = u->default_port;
        sin6->sin6_port = htons(u->default_port);
    }

    len = p - host;

    if (len == 0) {
        u->err = "no host";
        return false;
    }

    u->host_len = len + 2;
    u->host = host - 1;

    if (!pgrest_inet_inet6_addr((unsigned char *)host, 
                                len,
                                (unsigned char *)sin6->sin6_addr.s6_addr)) 
    {
        u->err = "invalid IPv6 address";
        return false;
    }

    if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
        u->wildcard = 1;
    }

    u->family = AF_INET6;
    u->naddrs = 1;

    u->addrs = palloc0(sizeof(pgrest_addr_t));

    sin6 = palloc0(sizeof(struct sockaddr_in6));

    memcpy(sin6, &u->sockaddr, sizeof(struct sockaddr_in6));

    u->addrs[0].sockaddr = (struct sockaddr *) sin6;
    u->addrs[0].socklen = sizeof(struct sockaddr_in6);

    p = palloc(u->host_len + sizeof(":65535"));

    memcpy(p, u->host, u->host_len);
    n = sprintf(p + u->host_len, ":%d", u->port);

    u->addrs[0].name_len = u->host_len + n;
    u->addrs[0].name = p;

    return true;
#else
    u->err = "the INET6 sockets are not supported on this platform";
    return false;
#endif
}

static bool 
pgrest_inet_parse_inet(pgrest_url_t *u)
{
    char                 *p, *host, *port, *last;
    size_t                len;
    int                   n;
    struct sockaddr_in   *sin;
#ifdef HAVE_IPV6
    struct sockaddr_in6  *sin6;
#endif

    u->socklen = sizeof(struct sockaddr_in);
    sin = (struct sockaddr_in *) &u->sockaddr;
    sin->sin_family = AF_INET;

    u->family = AF_INET;

    host = u->url;

    last = host + u->url_len;

    port = pgrest_strlchr(host, last, ':');

    if (port) {
        port++;

        len = last - port;

        n = pgrest_atoi(port, len);

        if (n < 1 || n > 65535) {
            u->err = "invalid port";
            return false;
        }

        u->port = (in_port_t) n;
        sin->sin_port = htons((in_port_t) n);

        u->port_text_len = len;
        u->port_text = port;

        last = port - 1;
    } else {
        if (u->listen) {
            /* test value as port only */
            n = pgrest_atoi(host, last - host);
            if (n < 1 || n > 65535) {
                u->err = "invalid port";
                return false;
            }

            u->port = (in_port_t) n;
            sin->sin_port = htons((in_port_t) n);

            u->port_text_len = last - host;
            u->port_text = host;

            u->wildcard = 1;

            return true;
        }

        u->no_port = 1;
        u->port = u->default_port;
        sin->sin_port = htons(u->default_port);
    }

    len = last - host;

    if (len == 0) {
        u->err = "no host";
        return false;
    }

    u->host_len = len;
    u->host = host;

    if (u->listen && len == 1 && *host == '*') {
        sin->sin_addr.s_addr = INADDR_ANY;
        u->wildcard = 1;
        return true;
    }

    sin->sin_addr.s_addr = pgrest_inet_inet_addr((unsigned char *)host, len);

    if (sin->sin_addr.s_addr != INADDR_NONE) {

        if (sin->sin_addr.s_addr == INADDR_ANY) {
            u->wildcard = 1;
        }

        u->naddrs = 1;

        u->addrs = palloc0(sizeof(pgrest_addr_t));
        sin = palloc0(sizeof(struct sockaddr_in));

        memcpy(sin, &u->sockaddr, sizeof(struct sockaddr_in));

        u->addrs[0].sockaddr = (struct sockaddr *) sin;
        u->addrs[0].socklen = sizeof(struct sockaddr_in);

        p = palloc(u->host_len + sizeof(":65535"));

        memcpy(p, u->host, u->host_len);
        n = sprintf(p + u->host_len, ":%d", u->port);

        u->addrs[0].name_len = u->host_len + n;
        u->addrs[0].name = p;

        return true;
    }

    if (u->no_resolve) {
        return true;
    }

    if (!pgrest_inet_resolve_host(u)) {
        return false;
    }

    u->family = u->addrs[0].sockaddr->sa_family;
    u->socklen = u->addrs[0].socklen;
    memcpy(&u->sockaddr, u->addrs[0].sockaddr, u->addrs[0].socklen);

    switch (u->family) {

#ifdef HAVE_IPV6
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) &u->sockaddr;

        if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
            u->wildcard = 1;
        }

        break;
#endif
    default: /* AF_INET */
        sin = (struct sockaddr_in *) &u->sockaddr;

        if (sin->sin_addr.s_addr == INADDR_ANY) {
            u->wildcard = 1;
        }

        break;
    }

    return true;
}

bool
pgrest_inet_parse_url(pgrest_url_t *u)
{
    char   *p;
    size_t  len;

    p = u->url;
    len = u->url_len;

    if (len >= 5 && strncasecmp(p, (char *) "unix:", 5) == 0) {
        return pgrest_inet_parse_unix(u);
    }

    if (len && p[0] == '[') {
        return pgrest_inet_parse_inet6(u);
    }

    return pgrest_inet_parse_inet(u);
}
