/**
 * Live wire-compatibility tests against the C++ server.
 *
 * Spawns ../build/redis-server (or $REDIS_SERVER_BIN) as a primary and a
 * replica on free ports in a temporary directory, then drives them through
 * the TypeScript RESP client and the bridge. Skipped if the binary is missing.
 */
import { spawn, type ChildProcess } from "node:child_process";
import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { afterAll, beforeAll, describe, expect, it } from "vitest";

import { executeCommand } from "../src/lib/bridge/execute";
import { BridgeSession } from "../src/lib/bridge/session";
import { RespConnection } from "../src/lib/resp/client";
import type { PushEvent } from "../src/lib/bridge/ws-protocol";

const BINARY = path.resolve(process.env.REDIS_SERVER_BIN ?? path.join(__dirname, "../../build/redis-server"));
const available = existsSync(BINARY);

function freePort(): Promise<number> {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      server.close(() => resolve(typeof address === "object" && address ? address.port : 0));
    });
  });
}

async function waitForPort(port: number, timeoutMs = 5000): Promise<void> {
  const deadline = Date.now() + timeoutMs;

  while (Date.now() < deadline) {
    const ok = await new Promise<boolean>((resolve) => {
      const socket = net.createConnection({ host: "127.0.0.1", port });
      socket.once("connect", () => {
        socket.destroy();
        resolve(true);
      });
      socket.once("error", () => resolve(false));
    });
    if (ok) return;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }

  throw new Error(`port ${port} did not open`);
}

async function eventually<T>(fn: () => Promise<T>, accept: (value: T) => boolean, timeoutMs = 3000): Promise<T> {
  const deadline = Date.now() + timeoutMs;
  let last = await fn();
  while (!accept(last) && Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 50));
    last = await fn();
  }
  return last;
}

describe.skipIf(!available)("live C++ server", () => {
  const children: ChildProcess[] = [];
  let dir = "";
  let primaryPort = 0;
  let replicaPort = 0;
  const connections: RespConnection[] = [];

  async function connect(port = primaryPort): Promise<RespConnection> {
    const connection = new RespConnection({ host: "127.0.0.1", port });
    connections.push(connection);
    await connection.connect();
    return connection;
  }

  function start(name: string, config: string, port: number): Promise<void> {
    const file = path.join(dir, `${name}.conf`);
    writeFileSync(file, config);
    const child = spawn(BINARY, [file], { cwd: dir, stdio: "ignore" });
    children.push(child);
    return waitForPort(port);
  }

  beforeAll(async () => {
    dir = mkdtempSync(path.join(os.tmpdir(), "rfs-dashboard-"));
    primaryPort = await freePort();
    replicaPort = await freePort();

    const common = "bind 127.0.0.1\nsave-on-shutdown no\nmax-memory 0\n";
    await start("primary", `${common}port ${primaryPort}\nrdb-file p.rdb\naof-file p.aof\n`, primaryPort);
    await start(
      "replica",
      `${common}port ${replicaPort}\nrdb-file r.rdb\naof-file r.aof\nreplicaof 127.0.0.1 ${primaryPort}\n`,
      replicaPort,
    );

    process.env.REDIS_HOST = "127.0.0.1";
    process.env.REDIS_PORT = String(primaryPort);
    process.env.REPLICA_HOST = "127.0.0.1";
    process.env.REPLICA_PORT = String(replicaPort);
  });

  afterAll(() => {
    for (const connection of connections) connection.close();
    for (const child of children) child.kill("SIGTERM");
    if (dir) rmSync(dir, { recursive: true, force: true });
  });

  it("round-trips basic commands with UTF-8 payloads", async () => {
    const connection = await connect();

    expect((await connection.execute(["PING"])).replies).toEqual([{ type: "simple-string", value: "PONG" }]);
    expect((await connection.execute(["SET", "greeting", "héllo wörld"])).rawResponse).toBe("+OK\r\n");

    const get = await connection.execute(["GET", "greeting"]);
    expect(get.replies).toEqual([{ type: "bulk-string", value: "héllo wörld" }]);
    expect(get.rawRequest).toBe("*2\r\n$3\r\nGET\r\n$8\r\ngreeting\r\n");
    expect(get.latencyMs).toBeGreaterThan(0);

    expect((await connection.execute(["GET", "missing"])).replies).toEqual([{ type: "null-bulk-string" }]);
  });

  it("returns Redis errors as replies", async () => {
    const connection = await connect();
    const result = await connection.execute(["SET", "only-key"]);
    expect(result.replies[0]).toEqual({ type: "error", value: "ERR wrong number of arguments for 'set' command" });
  });

  it("pipelines many commands in a single write", async () => {
    const connection = await connect();
    const commands = Array.from({ length: 200 }, (_, index) => ["RPUSH", "pipe:list", `item-${index}`]);
    const results = await connection.pipeline(commands);

    expect(results.map((result) => result.replies[0])).toEqual(
      commands.map((_, index) => ({ type: "integer", value: index + 1 })),
    );

    const range = await connection.execute(["LRANGE", "pipe:list", "0", "2"]);
    expect(range.replies[0]).toEqual({
      type: "array",
      value: ["item-0", "item-1", "item-2"].map((value) => ({ type: "bulk-string", value })),
    });
  });

  it("runs MULTI/EXEC on one connection and detects WATCH conflicts", async () => {
    const tx = await connect();
    const other = await connect();

    await tx.execute(["WATCH", "account"]);
    expect((await tx.execute(["MULTI"])).replies[0]).toEqual({ type: "simple-string", value: "OK" });
    expect((await tx.execute(["SET", "account", "10"])).replies[0]).toEqual({ type: "simple-string", value: "QUEUED" });
    expect((await tx.execute(["EXPIRE", "account", "60"])).replies[0]).toEqual({
      type: "simple-string",
      value: "QUEUED",
    });

    const exec = await tx.execute(["EXEC"]);
    expect(exec.replies[0]).toEqual({
      type: "array",
      value: [
        { type: "simple-string", value: "OK" },
        { type: "integer", value: 1 },
      ],
    });

    await tx.execute(["WATCH", "account"]);
    await tx.execute(["MULTI"]);
    await tx.execute(["SET", "account", "20"]);
    await other.execute(["SET", "account", "99"]);
    expect((await tx.execute(["EXEC"])).replies[0]).toEqual({ type: "null-array" });
    expect((await other.execute(["GET", "account"])).replies[0]).toEqual({ type: "bulk-string", value: "99" });
  });

  it("delivers Pub/Sub messages as pushes and counts multi-frame replies", async () => {
    const subscriber = await connect();
    const publisher = await connect();
    const pushes: string[] = [];
    subscriber.onPush((frame) => pushes.push(frame.raw.toString()));

    const subscribe = await subscriber.execute(["SUBSCRIBE", "news", "alerts"]);
    expect(subscribe.replies).toHaveLength(2);
    expect([...subscriber.subscriptions].sort()).toEqual(["alerts", "news"]);

    const publish = await publisher.execute(["PUBLISH", "news", "hello"]);
    expect(publish.replies[0]).toEqual({ type: "integer", value: 1 });

    await eventually(async () => pushes.length, (count) => count === 1);
    expect(pushes[0]).toBe("*3\r\n$7\r\nmessage\r\n$4\r\nnews\r\n$5\r\nhello\r\n");

    const unsubscribe = await subscriber.execute(["UNSUBSCRIBE"]);
    expect(unsubscribe.replies).toHaveLength(2);
    expect(subscriber.subscriptions).toEqual([]);

    const empty = await subscriber.execute(["UNSUBSCRIBE"]);
    expect(empty.replies).toHaveLength(1);
  });

  it("replicates writes and rejects writes on the replica", async () => {
    const replica = await connect(replicaPort);
    const readonly = await replica.execute(["SET", "replication:test", "x"]);
    expect(readonly.replies[0]).toEqual({
      type: "error",
      value: "READONLY You can't write against a read only replica.",
    });

    await (await connect()).execute(["SET", "replication:test", "hello"]);
    const replicated = await eventually(
      () => replica.execute(["GET", "replication:test"]),
      (result) => result.replies[0]?.type === "bulk-string",
    );
    expect(replicated.replies[0]).toEqual({ type: "bulk-string", value: "hello" });
  });

  it("bridge executeCommand maps outcomes and guards stateful commands", async () => {
    const ok = await executeCommand(["ECHO", "bridge"], "primary");
    expect(ok).toMatchObject({ ok: true, outcome: "reply", type: "bulk-string", value: "bridge" });

    const readonly = await executeCommand(["SET", "k", "v"], "replica");
    expect(readonly).toMatchObject({ ok: false, outcome: "redis-error" });

    const blocked = await executeCommand(["MULTI"], "primary");
    expect(blocked).toMatchObject({ ok: false, outcome: "failure", kind: "bad-request" });

    const info = await executeCommand(["INFO", "replication"], "primary");
    expect(info.outcome === "reply" && String(info.value)).toContain("connected_replicas:1");
  });

  it("bridge sessions mirror transaction and subscription state", async () => {
    const tx = new BridgeSession("tx", "primary", "transaction");
    const sub = new BridgeSession("sub", "primary", "subscriber");
    const pushes: PushEvent[] = [];
    await tx.open(() => undefined, () => undefined);
    await sub.open((push) => pushes.push(push), () => undefined);

    await tx.execute(["WATCH", "a", "b"]);
    await tx.execute(["MULTI"]);
    expect(tx.state).toEqual({ subscriptions: [], inMulti: true, watching: ["a", "b"] });
    await tx.execute(["DISCARD"]);
    expect(tx.state).toEqual({ subscriptions: [], inMulti: false, watching: [] });

    await sub.execute(["SUBSCRIBE", "live"]);
    expect(sub.state.subscriptions).toEqual(["live"]);
    await executeCommand(["PUBLISH", "live", "from-bridge"], "primary");
    await eventually(async () => pushes.length, (count) => count === 1);
    expect(pushes[0].message).toEqual({ channel: "live", payload: "from-bridge" });

    tx.close();
    sub.close();
  });

  it("reports offline servers as connection failures", async () => {
    const port = await freePort();
    const connection = new RespConnection({ host: "127.0.0.1", port, connectTimeoutMs: 500 });
    await expect(connection.connect()).rejects.toMatchObject({ kind: "connection" });
  });
});
