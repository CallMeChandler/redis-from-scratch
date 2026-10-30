# Redis From Scratch

A Redis-compatible in-memory database server built from scratch in modern C++20.

The project implements the major systems behind Redis rather than wrapping an existing Redis server: TCP networking, an epoll-based event loop, RESP2 parsing, typed in-memory data structures, expiration, transactions, persistence, Pub/Sub, memory eviction, replication, observability, and protocol hardening.

The goal of this project is educational systems engineering, not complete Redis feature parity.

## Architecture

```text
redis-cli / application
          |
          v
       TCP socket
          |
          v
   Linux epoll event loop
          |
          v
      Connection
   input/output buffers
          |
          v
      RESP2 parser
          |
          v
    CommandHandler
          |
     +----+----+
     |         |
     v         v
 Database   Persistence
     |       RDB + AOF
     |
     +--> TTL / expiration
     +--> transactions
     +--> LRU eviction
     +--> memory accounting

TcpServer
   |
   +--> Pub/Sub
   +--> primary-replica replication
   +--> INFO / runtime metrics
```

The server is single-threaded and event-driven. Client sockets are non-blocking and multiplexed through Linux `epoll`.

## Features

### Networking

The server uses POSIX TCP sockets with non-blocking I/O and Linux `epoll`.

It supports multiple concurrent clients, buffered reads and writes, partial TCP frames, pipelined commands, graceful disconnect handling, and SIGINT/SIGTERM shutdown through `signalfd`.

### RESP2 Protocol

A custom streaming RESP2 parser handles:

- Simple Strings
- Errors
- Integers
- Bulk Strings
- Arrays

The parser supports fragmented TCP input and command pipelining.

Protocol hardening includes maximum bulk-string sizes, maximum array sizes, maximum nesting depth, connection input-buffer limits, malformed frame rejection, and fuzz testing under AddressSanitizer and UndefinedBehaviorSanitizer.

### Data Types

Supported data structures include:

**Strings**

```text
SET
GET
```

**Hashes**

```text
HSET
HGET
```

**Lists**

```text
LPUSH
RPUSH
LPOP
RPOP
LRANGE
LLEN
```

**Sets**

```text
SADD
SREM
SISMEMBER
SMEMBERS
```

**Sorted Sets**

```text
ZADD
ZSCORE
ZRANGE
```

**Generic key operations**

```text
EXISTS
DEL
```

### Expiration

Keys can expire using:

```text
EXPIRE
EXPIREAT
TTL
```

Expiration uses both lazy deletion during key access and an active expiration cycle in the server event loop.

Persistence stores absolute expiration timestamps so downtime does not reset TTLs.

### Transactions

The server implements:

```text
MULTI
EXEC
DISCARD
WATCH
UNWATCH
```

`WATCH` uses per-key version numbers to provide optimistic concurrency control.

### Persistence

Two persistence mechanisms are implemented.

#### RDB snapshots

The custom binary RDB format serializes all supported Redis value types together with absolute expiration timestamps.

Snapshots are written through a temporary file and renamed after a successful write to avoid destroying the previous valid snapshot during an interrupted save.

#### Append Only File

Successful write commands are serialized as RESP and appended to an AOF.

On startup:

```text
non-empty AOF -> replay AOF
otherwise     -> load RDB
```

AOF rewrite reconstructs the current logical database into a compact new log.

Recovery can discard an incomplete final AOF command caused by an interrupted write while rejecting genuinely malformed corruption.

### Memory Accounting and LRU Eviction

The database tracks an approximate logical memory footprint for stored values.

```text
MEMORY USAGE <key>
```

A configurable memory ceiling is available:

```text
max-memory <bytes>
```

When the limit is exceeded, the server performs all-keys LRU eviction.

This implementation uses an exact logical access clock and scans keys to find the least recently used candidate. This is intentionally simpler than production Redis, which uses approximate LRU sampling for better scalability.

Evictions are persisted as `DEL` operations so deleted keys do not reappear after restart.

### Pub/Sub

RESP2 Pub/Sub support includes:

```text
SUBSCRIBE
UNSUBSCRIBE
PUBLISH
```

Subscriptions are maintained in memory and removed automatically when a client disconnects.

### Primary-Replica Replication

A server can be configured as a replica:

```text
replicaof 127.0.0.1 6379
```

A replica connects to its primary and performs a full synchronization of the current dataset, then consumes subsequent write commands as a replication stream.

Replicas are read-only for ordinary clients.

The implementation supports full synchronization and live write propagation. Partial resynchronization, replication backlogs, Sentinel, automatic failover, and distributed consensus are outside the scope of this project.

### Observability

The server implements:

```text
INFO
INFO server
INFO clients
INFO memory
INFO stats
INFO replication
INFO keyspace
```

Metrics include uptime, connected clients, connection counts, network bytes, commands processed, logical memory usage, expiration counts, eviction counts, key counts, and replication status.

## Building

Requirements:

```text
Linux
C++20 compiler
CMake >= 3.20
```

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

Run with default configuration:

```bash
./build/redis-server
```

Run with a configuration file:

```bash
./build/redis-server redis.conf.example
```

Connect using the official Redis CLI:

```bash
redis-cli -p 6379
```

Example:

```text
127.0.0.1:6379> SET user ritvik
OK

127.0.0.1:6379> GET user
"ritvik"

127.0.0.1:6379> RPUSH queue one
(integer) 1

127.0.0.1:6379> RPUSH queue two
(integer) 2

127.0.0.1:6379> LRANGE queue 0 -1
1) "one"
2) "two"
```

## Configuration

Example configuration:

```text
bind 0.0.0.0
port 6379

listen-backlog 128
max-clients 1024

rdb-file dump.rdb
aof-file appendonly.aof

epoll-timeout-ms 100
expire-batch-size 64

max-memory 0

save-on-shutdown yes
```

`max-memory 0` means unlimited memory.

Replica example:

```text
bind 127.0.0.1
port 6380

rdb-file replica.rdb
aof-file replica.aof

replicaof 127.0.0.1 6379
```

## Testing

Run the parser fuzz test:

```bash
./build/resp-fuzz
```

Run the integration smoke test while the server is running:

```bash
./scripts/smoke_test.sh
```

The RESP parser fuzz target uses deterministic randomized input and can be compiled with AddressSanitizer and UndefinedBehaviorSanitizer.

## Dashboard

`dashboard/` contains a Next.js playground for every implemented feature: data types, TTL, transactions, Pub/Sub, LRU eviction, replication, persistence, INFO metrics, and a raw RESP terminal. Its Node backend speaks RESP2 to the server over plain TCP with a hand-written TypeScript client, so it also exercises the server's wire compatibility.

```bash
./build/redis-server examples/primary.conf
./build/redis-server examples/replica.conf
cd dashboard && npm install && npm run dev    # http://localhost:3000
```

`docker compose up --build` runs primary, replica, and dashboard together. See [dashboard/README.md](dashboard/README.md) for architecture, environment variables, and Railway deployment.

## Benchmarking

Build an optimized version first:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
```

Start the release server:

```bash
./build-release/redis-server redis.conf.example
```

Run:

```bash
./benchmarks/run_benchmarks.sh
```

The benchmark exercises PING, SET, and GET using concurrent connections and pipelining through the official `redis-benchmark` client.

Performance numbers depend heavily on hardware, compiler optimization, persistence settings, client count, and pipeline depth, so benchmark results should always be reported together with those parameters.

## Design Decisions

The project intentionally favors transparent implementations that make the underlying systems concepts visible.

The event loop is single-threaded, which avoids internal locking for database mutations and provides deterministic command ordering.

Sorted sets currently use a hash map for member-to-score storage and perform sorting when range queries are requested. Production Redis instead uses more specialized structures such as skip lists.

LRU eviction uses an exact logical access counter and an O(N) victim scan. Production Redis uses approximate sampling to reduce eviction overhead.

The custom RDB format is designed for this project and is not compatible with Redis RDB files.

Replication uses Redis-like command streaming but does not implement Redis's full replication protocol.

Memory accounting estimates logical object memory rather than allocator-level RSS.

These tradeoffs keep the implementation understandable while still exposing the engineering problems behind a Redis-style server.

## Project Structure

```text
include/redis/
├── command/
├── config/
├── net/
├── persistence/
├── protocol/
├── pubsub/
├── replication/
└── storage/

src/
├── command/
├── config/
├── net/
├── persistence/
├── protocol/
├── pubsub/
├── replication/
└── storage/

tests/
benchmarks/
scripts/
examples/
```

## What This Project Covers

The implementation exercises several core systems concepts:

```text
POSIX sockets
non-blocking I/O
epoll
stream-oriented protocol parsing
event-driven architecture
in-memory data modeling
optimistic concurrency control
TTL management
binary serialization
append-only logging
crash recovery
LRU eviction
Pub/Sub
primary-replica synchronization
resource limits
fuzz testing
runtime observability
performance benchmarking
```

## Scope

This is a Redis-compatible learning implementation, not a drop-in replacement for production Redis.

Features such as Redis Cluster, Lua scripting, streams, ACLs, TLS, modules, Sentinel, partial replication resynchronization, diskless replication, and the complete Redis command set are intentionally outside the current scope.

## License

Add the license you want to use for the repository before distributing or reusing the project outside its current context.