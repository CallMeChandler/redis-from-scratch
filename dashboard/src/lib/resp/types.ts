/**
 * RESP2 value model shared by the Node bridge and the browser.
 *
 * Bulk strings are decoded as UTF-8 for display; lengths on the wire are
 * always byte lengths, which the parser handles on raw Buffers.
 */

export type RespValue =
  | { type: "simple-string"; value: string }
  | { type: "error"; value: string }
  | { type: "integer"; value: number }
  | { type: "bulk-string"; value: string }
  | { type: "null-bulk-string" }
  | { type: "array"; value: RespValue[] }
  | { type: "null-array" };

export type RespType = RespValue["type"];

/** Plain JSON projection of a RESP value (what a typical client would return). */
export type PlainValue = string | number | null | PlainValue[];

export type Target = "primary" | "replica";

export const TARGETS: readonly Target[] = ["primary", "replica"];

export function isTarget(value: unknown): value is Target {
  return value === "primary" || value === "replica";
}

/** A completed round trip: request bytes out, one or more reply frames back. */
export interface CommandExecution {
  id: string;
  target: Target;
  /** "http" = shared pooled socket, "session" = dedicated per-session socket. */
  via: "http" | "session";
  sessionId?: string;
  command: string[];
  rawRequest: string;
  rawResponse: string;
  /** Usually exactly one frame; SUBSCRIBE/UNSUBSCRIBE may produce several. */
  replies: RespValue[];
  latencyMs: number;
  startedAt: number;
}

export interface CommandReplyResponse extends CommandExecution {
  ok: true;
  outcome: "reply";
  type: RespType;
  value: PlainValue;
}

export interface CommandRedisErrorResponse extends CommandExecution {
  ok: false;
  outcome: "redis-error";
  type: "error";
  redisError: string;
}

export type FailureKind =
  | "bad-request"
  | "not-configured"
  | "connection"
  | "timeout"
  | "protocol";

export interface CommandFailureResponse {
  ok: false;
  outcome: "failure";
  id: string;
  kind: FailureKind;
  error: string;
  target: Target;
  via: "http" | "session";
  sessionId?: string;
  command: string[];
  rawRequest?: string;
  startedAt: number;
}

/** Central response type for every command the dashboard runs. */
export type CommandResponse =
  | CommandReplyResponse
  | CommandRedisErrorResponse
  | CommandFailureResponse;

/** Responses that actually reached the server and came back with RESP frames. */
export type CommandRoundTrip = CommandReplyResponse | CommandRedisErrorResponse;

export function isRoundTrip(
  response: CommandResponse,
): response is CommandRoundTrip {
  return response.outcome !== "failure";
}

export type HealthStatus = "online" | "offline" | "not-configured";

export interface EndpointHealth {
  target: Target;
  status: HealthStatus;
  host: string | null;
  port: number | null;
  latencyMs?: number;
  error?: string;
}

export interface HealthResponse {
  checkedAt: number;
  primary: EndpointHealth;
  replica: EndpointHealth;
}

export type InfoSections = Record<string, Record<string, string>>;

export type InfoResponse =
  | {
      ok: true;
      target: Target;
      sections: InfoSections;
      raw: string;
      execution: CommandReplyResponse;
    }
  | {
      ok: false;
      target: Target;
      error: string;
      response: CommandResponse;
    };
