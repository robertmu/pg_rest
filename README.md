# About

The pg_rest extension provides HTTP interface for PostgreSQL. It provides
a cleaner, more standards-compliant, faster API than you are likely to 
write from scratch. It is based on background worker(PostgreSQL 9.3+).

# Performance

If you're used to servers written in interpreted languages (or named
after precious gems), prepare to be pleasantly surprised by pg_rest 
performance.

Four factors contribute to the speed.

* Muliple worker processes, configurable number of worker processes 
* Each worker process is single-threaded and runs independently(event-driven + non-blocking)
* Keeping a pool of db connections (like [pgbouncer](https://github.com/bouncer/bouncer))
* PreparedStatement caching (avoid the hard parsing for sql)

Ultimately the server (when load balanced) is constrained by database
performance. 

# Features
* JSON output by default, optional JSONP parameter (`?jsonp=myFunction` or `?callback=myFunction`).
* Pub/Sub using `Transfer-Encoding: chunked`|`WebSocket`, works with JSONP as well. pg_rest can be used as a Comet server.
* Realtime Database, http client can receiving push notifications from pg_rest(like Firebase, PostgreSQL 9.4+(logical decoding)).
* HTTP 1.1 pipelining, HTTP/2 (70,000 http requests per second on a desktop Linux machine.)
* Multi-process server, configurable number of worker processes.
* WebSocket support.
* Connects to PostgreSQL using a TCP or UNIX socket.
* Restricted commands by IP range (CIDR subnet + mask) or HTTP Basic Auth, returning 403 errors.
* Custom Content-Type using a pre-defined file extension, or with `?type=some/thing`.
* URL-encoded parameters for binary data or slashes and question marks. For instance, `%2f` is decoded as `/` but not used as a command separator.
* Cross-origin requests, usable with XMLHttpRequest2 (Cross-Origin Resource Sharing - CORS).

# Ideas, TODO...
* mruby handler(http application server)
