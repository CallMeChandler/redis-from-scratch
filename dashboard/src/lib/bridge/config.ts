import type { Target } from "../resp/types";

export interface Endpoint {
  host: string;
  port: number;
}

const DISABLED = new Set(["", "none", "off", "false", "disabled"]);

function readPort(name: string, fallback: number): number {
  const raw = process.env[name];
  if (raw === undefined || raw.trim() === "") return fallback;

  const port = Number(raw);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error(`${name} must be an integer between 1 and 65535, got ${JSON.stringify(raw)}`);
  }

  return port;
}

/**
 * Resolves where each target lives.
 *
 *   REDIS_HOST / REDIS_PORT        primary, default 127.0.0.1:6379
 *   REPLICA_HOST / REPLICA_PORT    replica, default 127.0.0.1:6380
 *
 * Setting REPLICA_HOST to an empty string or "none" marks the replica as
 * not configured, so the UI shows NOT CONFIGURED instead of OFFLINE.
 */
export function getEndpoint(target: Target): Endpoint | null {
  if (target === "primary") {
    return {
      host: process.env.REDIS_HOST?.trim() || "127.0.0.1",
      port: readPort("REDIS_PORT", 6379),
    };
  }

  const host = process.env.REPLICA_HOST;
  if (host !== undefined && DISABLED.has(host.trim().toLowerCase())) return null;

  return {
    host: host?.trim() || "127.0.0.1",
    port: readPort("REPLICA_PORT", 6380),
  };
}

export const COMMAND_TIMEOUT_MS = Number(process.env.REDIS_COMMAND_TIMEOUT_MS) || 5000;
export const CONNECT_TIMEOUT_MS = Number(process.env.REDIS_CONNECT_TIMEOUT_MS) || 2000;
