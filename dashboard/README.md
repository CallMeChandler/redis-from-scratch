# Redis From Scratch — Dashboard

A web playground for the C++ server in this repository. Every command the dashboard runs is
encoded as RESP2 by a small TypeScript client and sent over a raw TCP socket, so the dashboard
doubles as a wire-compatibility test for the server's protocol implementation.

No Redis client library is used, and `redis-cli` is never shelled out to.

## Architecture

```text
Browser ──HTTP  /api/command · /api/info · /api/health ──▶ Next.js route handlers ──▶ pooled socket per target
Browser ──WebSocket /ws (one per tab, multiplexed) ──────▶ custom Node server ─────▶ one socket per session
                                                           │
                                                           ▼
                                          src/lib/resp: encoder · streaming parser · net.Socket client
                                                           │  RESP2 over TCP
                                                           ▼
                                          C++ redis-server (primary :6379, replica :6380)
```

- **Stateless commands** (data-type pages, INFO, health) go through `POST /api/command`, which
  uses one long-lived socket per target. Commands whose effect is tied to a connection —
  `MULTI EXEC DISCARD WATCH UNWATCH SUBSCRIBE UNSUBSCRIBE` — are rejected there with HTTP 400,
  so the shared socket can never be left inside a transaction or in subscribe mode.
- **Sessions** (Raw CLI, Transactions, each Pub/Sub subscriber) each own a dedicated TCP socket
  held by the Node server. The browser talks to them over a single WebSocket.
- `SYNC`/`PSYNC` are refused everywhere because they would turn a dashboard socket into a
  replication stream.

```text
src/lib/resp/      types, encoder, parser (incremental, byte-accurate), client (net.Socket), format, tokenize
src/lib/bridge/    config (env), pool, execute (executeCommand), session, ws-protocol, ws-server
src/lib/client/    browser: fetch helpers, BridgeSocket (WebSocket multiplexer)
src/hooks/         useCommandRunner, useSession, useInfo, useSnapshot
src/components/    ui/ primitives · command/ reusable viewers & forms · shell/ layout · features/<page>/
server.ts          custom server: Next.js + WebSocket upgrade on /ws
tests/             parser/encoder/format unit tests + live integration tests
```

## Running locally

From the repository root, build and start the C++ servers:

```bash
cmake -S . -B build && cmake --build build -j
./build/redis-server examples/primary.conf     # :6379
./build/redis-server examples/replica.conf     # :6380, replicaof 127.0.0.1 6379
```

Then start the dashboard:

```bash
cd dashboard
npm install
npm run dev            # http://localhost:3000  (custom server with WebSocket bridge)
```

Production mode:

```bash
npm run build          # next build + compile server.ts to dist/
npm start              # node dist/server.js
```

`npm run dev` runs `server.ts` through `tsx`. Page and component edits hot-reload; edits to
`src/lib/bridge` or `src/lib/resp` used by the WebSocket server need a restart.

### LRU experiment

The server has no `CONFIG SET`, so `max-memory` comes from the config file. To see evictions,
start the primary with the bundled demo config:

```bash
./build/redis-server dashboard/dev/lru.conf    # max-memory 3000, port 6379
```

The Memory / LRU page suggests a value size from `maxmemory` (each key costs roughly the value
plus about 125 bytes in the server's accounting).

## Environment variables

| Variable | Default | Meaning |
| --- | --- | --- |
| `REDIS_HOST` | `127.0.0.1` | Primary host (hostname or IP) |
| `REDIS_PORT` | `6379` | Primary port |
| `REPLICA_HOST` | `127.0.0.1` | Replica host; `none` or empty → shown as NOT CONFIGURED |
| `REPLICA_PORT` | `6380` | Replica port |
| `PORT` | `3000` | Dashboard HTTP/WebSocket port |
| `REDIS_CONNECT_TIMEOUT_MS` | `2000` | TCP connect timeout |
| `REDIS_COMMAND_TIMEOUT_MS` | `5000` | Per-command reply timeout |

## API

`POST /api/command`

```json
{ "target": "primary", "args": ["SET", "user", "ritvik"] }
```

```json
{
  "ok": true,
  "outcome": "reply",
  "command": ["SET", "user", "ritvik"],
  "rawRequest": "*3\r\n$3\r\nSET\r\n$4\r\nuser\r\n$6\r\nritvik\r\n",
  "rawResponse": "+OK\r\n",
  "replies": [{ "type": "simple-string", "value": "OK" }],
  "type": "simple-string",
  "value": "OK",
  "latencyMs": 0.72
}
```

Redis errors are replies, not HTTP failures (status 200):

```json
{ "ok": false, "outcome": "redis-error", "redisError": "READONLY You can't write against a read only replica.", "...": "..." }
```

Network problems use `outcome: "failure"` with `kind` and an HTTP status:
`bad-request` 400 · `not-configured` 503 · `connection`/`protocol` 502 · `timeout` 504.

`GET /api/info?target=primary&section=memory` returns INFO parsed into sections.
`GET /api/health` PINGs both targets and reports `online` / `offline` / `not-configured`.

## How Pub/Sub works

1. The Pub/Sub page opens a **subscriber session** over the tab's WebSocket.
2. The Node server opens a dedicated `net.Socket` to the C++ server for it.
3. `SUBSCRIBE news alerts` goes out on that socket. The client expects one confirmation frame
   per channel (bare `UNSUBSCRIBE` expects one per current subscription) and tracks the
   subscribed channels from those confirmations.
4. Frames shaped `["message", channel, payload]` that arrive while subscribed are treated as
   pushes, not replies, and are forwarded to the browser as `session.push` messages with the
   raw RESP bytes.
5. `PUBLISH` from the publisher panel uses the stateless HTTP socket, a different connection,
   so the message really travels through the server's fan-out.

Closing the panel or the tab closes the subscriber socket.

## How transaction sessions work

`BridgeSession` (`src/lib/bridge/session.ts`) wraps one `RespConnection`. Every command from
the Transactions page goes to that same socket, in order: `WATCH`, `MULTI`, each queued command
(`+QUEUED`), then `EXEC` or `DISCARD`. The session mirrors connection state from the server's
replies (in MULTI, watched keys) and sends it with every result so the UI can render it. The
"concurrent client" button writes the watched key over the HTTP socket, after which `EXEC`
returns `*-1`.

Sessions live as long as the WebSocket that opened them (heartbeat every 30 s); at most 16 per tab.

## Testing

```bash
npm test               # vitest
npm run lint
npm run typecheck
```

`tests/integration.test.ts` spawns `../build/redis-server` as a primary and a replica on free
ports in a temp directory. It checks round trips, pipelining (200 commands in one write),
MULTI/EXEC with WATCH conflicts, Pub/Sub pushes, replication and READONLY, and the bridge
session state. It is skipped if the binary has not been built (`REDIS_SERVER_BIN` overrides the
path).

## Deployment

### Docker Compose (local)

```bash
docker compose up --build      # from the repository root → http://localhost:3000
```

The replica's `replicaof` must be an IPv4 literal (the server resolves it with `inet_pton`, not
DNS), so compose pins the primary to `172.28.0.10` on a dedicated network.

### Railway

Recommended: **one service from the all-in-one image** (`docker/all-in-one/Dockerfile`). It runs
the primary, the replica and the dashboard in one container over `127.0.0.1`, which avoids two
constraints of the C++ server: `replicaof` needs an IPv4 literal, and `bind` is IPv4-only.

1. Push the repository to GitHub.
2. In Railway: **New Project → Deploy from GitHub repo**.
3. In the service's **Variables**, set `RAILWAY_DOCKERFILE_PATH=docker/all-in-one/Dockerfile`.
4. **Settings → Networking → Generate Domain**. Railway injects `PORT`, and the server listens
   on it.
5. Optional persistence: add a **Volume** mounted at `/data`. Railway mounts volumes as root,
   so also set `RAILWAY_RUN_UID=0`.

Alternative, separate services: deploy the C++ server from the root `Dockerfile` and the dashboard
from `dashboard/` (its `Dockerfile` is picked up automatically). On the dashboard set
`REDIS_HOST=${{<primary-service>.RAILWAY_PRIVATE_DOMAIN}}`, `REDIS_PORT=6379` and
`REPLICA_HOST=none`. This only works if your Railway environment's private network reaches the
server over IPv4. A separate replica service is not possible without a stable IPv4 address for
`replicaof`.

## Limitations

- **No authentication.** Anyone who can reach the dashboard can run any implemented command
  (including `SAVE`/`REWRITEAOF`). Don't expose a deployment you care about publicly.
- Bulk strings are decoded as UTF-8 for display; non-UTF-8 binary values show replacement
  characters (their byte lengths on the wire are still handled correctly).
- Integers are JavaScript numbers; values beyond ±2^53 would lose precision.
- `max-memory` cannot be changed from the dashboard (the server has no `CONFIG SET`).
- The replica connects to its primary only at startup (`TcpServer::connectToPrimary` runs once),
  so after restarting the primary — e.g. to switch to `dev/lru.conf` — restart the replica too.
  The Replication page shows `primary_link_status:down` until you do.
- Health checks, INFO refreshes, snapshots and the TTL watcher are real commands, so they show
  up in `total_commands_processed`. They're labelled `auto` in the event log, which hides them
  by default.
