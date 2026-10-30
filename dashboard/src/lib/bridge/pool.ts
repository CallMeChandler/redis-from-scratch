import { RespConnection } from "../resp/client";
import type { Target } from "../resp/types";
import { COMMAND_TIMEOUT_MS, CONNECT_TIMEOUT_MS, type Endpoint } from "./config";

/**
 * One long-lived socket per target for stateless HTTP commands.
 *
 * Stored on globalThis so Next.js dev hot reloads don't leak sockets.
 * A closed socket (server restarted, timeout) is replaced on next use.
 */
const globalPool = globalThis as typeof globalThis & {
  __respPool?: Map<Target, RespConnection>;
};

const pool = (globalPool.__respPool ??= new Map<Target, RespConnection>());

export async function getPooledConnection(target: Target, endpoint: Endpoint): Promise<RespConnection> {
  const existing = pool.get(target);

  if (
    existing &&
    existing.state !== "closed" &&
    existing.host === endpoint.host &&
    existing.port === endpoint.port
  ) {
    await existing.connect();
    return existing;
  }

  existing?.close();

  const connection = new RespConnection({
    host: endpoint.host,
    port: endpoint.port,
    connectTimeoutMs: CONNECT_TIMEOUT_MS,
    commandTimeoutMs: COMMAND_TIMEOUT_MS,
  });

  pool.set(target, connection);
  await connection.connect();
  return connection;
}
