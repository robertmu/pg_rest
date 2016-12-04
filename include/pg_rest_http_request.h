/*-------------------------------------------------------------------------
 *
 * include/pg_rest_http_request.h
 *
 * http request handler.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_REST_HTTP_REQUEST_H
#define PG_REST_HTTP_REQUEST_H

#define PGREST_HTTP_LINGERING_BUFFER_SIZE     4096

#define PGREST_HTTP_VERSION_9                 9
#define PGREST_HTTP_VERSION_10                1000
#define PGREST_HTTP_VERSION_11                1001
#define PGREST_HTTP_VERSION_20                2000

#define PGREST_HTTP_UNKNOWN                   0x0001
#define PGREST_HTTP_GET                       0x0002
#define PGREST_HTTP_HEAD                      0x0004
#define PGREST_HTTP_POST                      0x0008
#define PGREST_HTTP_PUT                       0x0010
#define PGREST_HTTP_DELETE                    0x0020
#define PGREST_HTTP_MKCOL                     0x0040
#define PGREST_HTTP_COPY                      0x0080
#define PGREST_HTTP_MOVE                      0x0100
#define PGREST_HTTP_OPTIONS                   0x0200
#define PGREST_HTTP_PROPFIND                  0x0400
#define PGREST_HTTP_PROPPATCH                 0x0800
#define PGREST_HTTP_LOCK                      0x1000
#define PGREST_HTTP_UNLOCK                    0x2000
#define PGREST_HTTP_PATCH                     0x4000
#define PGREST_HTTP_TRACE                     0x8000

#define PGREST_HTTP_CONNECTION_CLOSE          1
#define PGREST_HTTP_CONNECTION_KEEP_ALIVE     2

#define PGREST_HTTP_CONTINUE                  100
#define PGREST_HTTP_SWITCHING_PROTOCOLS       101
#define PGREST_HTTP_PROCESSING                102

#define PGREST_HTTP_OK                        200
#define PGREST_HTTP_CREATED                   201
#define PGREST_HTTP_ACCEPTED                  202
#define PGREST_HTTP_NO_CONTENT                204
#define PGREST_HTTP_PARTIAL_CONTENT           206

#define PGREST_HTTP_SPECIAL_RESPONSE          300
#define PGREST_HTTP_MOVED_PERMANENTLY         301
#define PGREST_HTTP_MOVED_TEMPORARILY         302
#define PGREST_HTTP_SEE_OTHER                 303
#define PGREST_HTTP_NOT_MODIFIED              304
#define PGREST_HTTP_TEMPORARY_REDIRECT        307

#define PGREST_HTTP_BAD_REQUEST               400
#define PGREST_HTTP_UNAUTHORIZED              401
#define PGREST_HTTP_FORBIDDEN                 403
#define PGREST_HTTP_NOT_FOUND                 404
#define PGREST_HTTP_NOT_ALLOWED               405
#define PGREST_HTTP_REQUEST_TIME_OUT          408
#define PGREST_HTTP_CONFLICT                  409
#define PGREST_HTTP_LENGTH_REQUIRED           411
#define PGREST_HTTP_PRECONDITION_FAILED       412
#define PGREST_HTTP_REQUEST_ENTITY_TOO_LARGE  413
#define PGREST_HTTP_REQUEST_URI_TOO_LARGE     414
#define PGREST_HTTP_UNSUPPORTED_MEDIA_TYPE    415
#define PGREST_HTTP_RANGE_NOT_SATISFIABLE     416
#define PGREST_HTTP_MISDIRECTED_REQUEST       421

#define PGREST_HTTP_CLOSE                     444
#define PGREST_HTTP_REQUEST_HEADER_TOO_LARGE  494
#define PGREST_HTTPS_CERT_ERROR               495
#define PGREST_HTTPS_NO_CERT                  496

#define PGREST_HTTP_CLIENT_CLOSED_REQUEST     499
#define PGREST_HTTP_INTERNAL_SERVER_ERROR     500
#define PGREST_HTTP_NOT_IMPLEMENTED           501
#define PGREST_HTTP_BAD_GATEWAY               502
#define PGREST_HTTP_SERVICE_UNAVAILABLE       503
#define PGREST_HTTP_GATEWAY_TIME_OUT          504
#define PGREST_HTTP_INSUFFICIENT_STORAGE      507

typedef enum {
    PGREST_HTTP_INITING_REQUEST_STATE = 0,
    PGREST_HTTP_READING_REQUEST_STATE,
    PGREST_HTTP_PROCESS_REQUEST_STATE,

    PGREST_HTTP_CONNECT_UPSTREAM_STATE,
    PGREST_HTTP_WRITING_UPSTREAM_STATE,
    PGREST_HTTP_READING_UPSTREAM_STATE,

    PGREST_HTTP_WRITING_REQUEST_STATE,
    PGREST_HTTP_LINGERING_CLOSE_STATE,
    PGREST_HTTP_KEEPALIVE_STATE
} pgrest_http_state_e;


/**
 * HTTP connection
 */
typedef struct {
    pgrest_http_addr_conf_t          *addr_conf;
    size_t                            prev_size;
#if defined(HAVE_OPENSSL) && defined(SSL_CTRL_SET_TLSEXT_HOSTNAME)
    pgrest_string_t                  *ssl_servername;
#endif
#ifdef HAVE_OPENSSL
    unsigned                          ssl:1;
#endif
    unsigned                          parse:1;
} pgrest_http_connection_t;

typedef struct {
    pgrest_buffer_t                  *buffer;
    pgrest_iovec_t                    entity;
    off_t                             rest;
    struct phr_chunked_decoder       *chunked;
    size_t                            decode_length;
    unsigned                          split:1;
} pgrest_http_request_body_t;

typedef struct {
    pgrest_array_t                    headers;

    pgrest_param_t                   *host;
    pgrest_param_t                   *connection;
    pgrest_param_t                   *user_agent;
    pgrest_param_t                   *content_length;
    pgrest_param_t                   *content_range;
    pgrest_param_t                   *content_type;
    pgrest_param_t                   *range;
    pgrest_param_t                   *if_range;
    pgrest_param_t                   *transfer_encoding;
    pgrest_param_t                   *expect;
    pgrest_param_t                   *upgrade;
    pgrest_param_t                   *authorization;
    pgrest_param_t                   *keep_alive;
    pgrest_string_t                   user;
    pgrest_string_t                   passwd;
    pgrest_string_t                   server;

    off_t                             content_length_n;
    time_t                            keep_alive_n;

    unsigned                          connection_type:2;
    unsigned                          chunked:1;
} pgrest_http_headers_in_t;

typedef struct {
    pgrest_array_t                    headers;

    pgrest_uint_t                     status;
    pgrest_string_t                   status_line;

    off_t                             content_length_n;
    off_t                             content_offset;
    time_t                            date_time;
    time_t                            last_modified_time;
} pgrest_http_headers_out_t;

typedef void (*pgrest_http_event_handler_pt) (pgrest_http_request_t *req);

/**
 * HTTP request
 */
struct pgrest_http_request_s {
    /* the underlying connection */
    pgrest_connection_t              *conn;

    pgrest_http_event_handler_pt      read_event_handler;
    pgrest_http_event_handler_pt      write_event_handler;

    /* the HTTP version (represented as 0xMMmm (M=major, m=minor)) */
    int                               http_version;
    /* method */
    int                               method;
    /* abs-path of the request */
    pgrest_string_t                   path;

    pgrest_http_upstream_t           *upstream;
    pgrest_array_t                   *upstream_states;
    /* of pgrest_http_upstream_state_t */

    pgrest_mpool_t                   *pool;
    pgrest_buffer_t                  *header_in;

    pgrest_http_headers_in_t          headers_in;
    pgrest_http_headers_out_t         headers_out;

    pgrest_http_request_body_t       *request_body;
    time_t                            lingering_time;
    struct timeval                    start_time;

    pgrest_http_connection_t         *http_conn;
    pgrest_conf_http_server_t        *http_conf;

    pgrest_chain_t                   *out;
    size_t                            request_length;

    unsigned                          count:16;
    unsigned                          http_state:4;

    unsigned                          pipeline:1;
    unsigned                          chunked:1;
    unsigned                          header_only:1;
    unsigned                          keepalive:1;
    unsigned                          lingering_close:1;
    unsigned                          expect_tested:1;
    unsigned                          done:1;
};

void pgrest_http_close_request(pgrest_http_request_t *req, pgrest_int_t rc);
bool pgrest_http_set_virtual_server(pgrest_http_request_t *req, 
                                    pgrest_string_t       *host);
void pgrest_http_finalize_request(pgrest_http_request_t *req, pgrest_int_t rc);
int  pgrest_http_extend_header_buffer(pgrest_http_request_t *req);
int  pgrest_http_extend_body_buffer(pgrest_http_request_t *req);
bool pgrest_http_test_expect(pgrest_http_request_t *req);
void pgrest_http_block_reading(pgrest_http_request_t *req);
bool
pgrest_http_process_host(pgrest_http_request_t *req, 
                         pgrest_param_t        *header,
                         size_t                 offset);


#endif /* PG_REST_HTTP_REQUEST_H */
