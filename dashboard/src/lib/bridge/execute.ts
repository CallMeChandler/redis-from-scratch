import { randomUUID } from "node:crypto";

import { RespConnectionError, type RespConnection, type RoundTrip } from "../resp/client";
import { encodeCommand } from "../resp/encoder";
import { toPlain } from "../resp/format";
import type {
  CommandFailureResponse,
  CommandResponse,
  FailureKind,
  Target,
} from "../resp/types";
import { getEndpoint } from "./config";
import { getPooledConnection } from "./pool";

export const MAX_ARGS = 1024;
export const MAX_ARG_BYTES = 8 * 1024 * 1024;

/**
 * Commands whose effect is bound to the TCP connection. Sending them over
 * the shared pooled socket would leak state into every other request, so
 * they are only accepted on dedicated sessions (Raw CLI, Transactions,
 * Pub/Sub), which own their socket.
 */
export const CONNECTION_STATEFUL_COMMANDS = new Set([
  "MULTI",
  "EXEC",
  "DISCARD",
  "WATCH",
  "UNWATCH",
  "SUBSCRIBE",
  "UNSUBSCRIBE",
]);

/** Turns any connection into a replication stream; never allowed from the dashboard. */
export const FORBIDDEN_COMMANDS = new Set(["SYNC", "PSYNC"]);

interface Context {
  target: Target;
  via: "http" | "session";
  sessionId?: string;
}

export function validateArgs(args: unknown): string | null {
  if (!Array.isArray(args) || args.length === 0) return "args must be a non-empty array of strings";
  if (args.length > MAX_ARGS) return `at most ${MAX_ARGS} arguments are allowed`;
  if (!args.every((arg) => typeof arg === "string")) return "every argument must be a string";
  if (args[0].trim() === "") return "command name must not be empty";

  const bytes = args.reduce((total: number, arg: string) => total + Buffer.byteLength(arg), 0);
  if (bytes > MAX_ARG_BYTES) return `arguments exceed ${MAX_ARG_BYTES} bytes`;

  return null;
}

export function failure(
  context: Context,
  command: string[],
  kind: FailureKind,
  error: string,
): CommandFailureResponse {
  return {
    ok: false,
    outcome: "failure",
    id: randomUUID(),
    kind,
    error,
    target: context.target,
    via: context.via,
    sessionId: context.sessionId,
    command,
    rawRequest: kind === "bad-request" ? undefined : encodeCommand(command).toString("utf8"),
    startedAt: Date.now(),
  };
}

export function fromRoundTrip(context: Context, command: string[], trip: RoundTrip): CommandResponse {
  const base = {
    id: randomUUID(),
    target: context.target,
    via: context.via,
    sessionId: context.sessionId,
    command,
    rawRequest: trip.rawRequest,
    rawResponse: trip.rawResponse,
    replies: trip.replies,
    latencyMs: trip.latencyMs,
    startedAt: trip.startedAt,
  };

  const errorReply = trip.replies.find((reply) => reply.type === "error");

  if (errorReply?.type === "error") {
    return { ...base, ok: false, outcome: "redis-error", type: "error", redisError: errorReply.value };
  }

  const primary = trip.replies.length === 1 ? trip.replies[0] : { type: "array" as const, value: trip.replies };

  return { ...base, ok: true, outcome: "reply", type: primary.type, value: toPlain(primary) };
}

/** Runs a command on an already-open connection and maps the outcome. */
export async function runOn(
  connection: RespConnection,
  context: Context,
  command: string[],
): Promise<CommandResponse> {
  try {
    return fromRoundTrip(context, command, await connection.execute(command));
  } catch (error) {
    if (error instanceof RespConnectionError) return failure(context, command, error.kind, error.message);
    throw error;
  }
}

/**
 * Executes one stateless command against the primary or replica over the
 * shared pooled socket. Redis errors (e.g. READONLY) are returned as
 * `outcome: "redis-error"`; only network/protocol trouble is a failure.
 */
export async function executeCommand(args: string[], target: Target = "primary"): Promise<CommandResponse> {
  const context: Context = { target, via: "http" };
  const invalid = validateArgs(args);
  if (invalid) return failure(context, Array.isArray(args) ? args : [], "bad-request", invalid);

  const name = args[0].toUpperCase();

  if (FORBIDDEN_COMMANDS.has(name)) {
    return failure(context, args, "bad-request", `${name} would turn the dashboard connection into a replication stream`);
  }

  if (CONNECTION_STATEFUL_COMMANDS.has(name)) {
    return failure(
      context,
      args,
      "bad-request",
      `${name} depends on connection state; use a session (Raw CLI, Transactions or Pub/Sub) instead of the stateless API`,
    );
  }

  const endpoint = getEndpoint(target);
  if (endpoint === null) return failure(context, args, "not-configured", `${target} is not configured`);

  try {
    const connection = await getPooledConnection(target, endpoint);
    return await runOn(connection, context, args);
  } catch (error) {
    if (error instanceof RespConnectionError) return failure(context, args, error.kind, error.message);
    throw error;
  }
}

export function httpStatusFor(response: CommandResponse): number {
  if (response.outcome !== "failure") return 200;

  switch (response.kind) {
    case "bad-request":
      return 400;
    case "not-configured":
      return 503;
    case "timeout":
      return 504;
    case "connection":
    case "protocol":
      return 502;
  }
}
