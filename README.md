[![Build Status](https://travis-ci.org/robertmu/pg_rest.svg?branch=develop)](https://travis-ci.org/robertmu/pg_rest)

----------------------------------------------------------------------

# About

The pg_rest extension provides HTTP interface for PostgreSQL. It provides
a cleaner, more standards-compliant, faster API than you are likely to 
write from scratch. It is based on background worker(PostgreSQL 9.3+).
It is not finished and is still in the development/experimental stage

# Performance

If you're used to servers written in interpreted languages (or named
after precious gems), prepare to be pleasantly surprised by pg_rest 
performance.

Four factors contribute to the speed.

* Muliple worker processes, configurable number of worker processes 
* Each worker process is single-threaded and runs independently(event-driven + non-blocking)
* Keeping a pool of db connections (like [pgbouncer](https://github.com/bouncer/bouncer))
* PreparedStatement caching (avoid the SQL hard parsing)
* Serializing JSON responses directly in SQL

Ultimately the server (when load balanced) is constrained by database
performance. 

# Features
* JSON output by default, optional JSONP parameter (`?jsonp=myFunction` or `?callback=myFunction`).
* Realtime Database, http client can receiving push notifications from pg_rest(need PostgreSQL 9.4+(logical decoding), like [Firebase](https://www.firebase.com/),[RethinkDB](https://github.com/rethinkdb/rethinkdb)).
* Authorizatio, npg_rest handles authentication (via [JSON Web Tokens](http://postgrest.com/admin/security/#json-web-tokens)) and delegates authorization to the role information defined in the database
* HTTP 1.1 pipelining, HTTP/2 (?0,000 http requests per second on a desktop Linux machine.)
* Read/Write Splitting
* Pub/Sub using `Transfer-Encoding: chunked`|`WebSocket`, works with JSONP as well. pg_rest can be used as a Comet server.
* Multi-process server, configurable number of worker processes.
* WebSocket,OpenSSL support.
* Connects to PostgreSQL using a TCP or UNIX socket.
* Custom Content-Type using a pre-defined file extension, or with `?type=some/thing`.

# Ideas, TODO...
* mruby handler(http application server)
