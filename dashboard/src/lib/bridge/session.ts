import { randomUUID } from "node:crypto";

import { RespConnection, isMessagePush } from "../resp/client";
import type { CommandResponse, RespValue, Target } from "../resp/types";
import { COMMAND_TIMEOUT_MS, CONNECT_TIMEOUT_MS, getEndpoint } from "./config";
import { FORBIDDEN_COMMANDS, failure, runOn, validateArgs } from "./execute";
import type { PushEvent, SessionKind, SessionState } from "./ws-protocol";

function bulk(value: RespValue | undefined): string {
  return value?.type === "bulk-string" ? value.value : "";
}

/**
 * A dedicated TCP connection owned by one browser-side session.
 *
 * Everything sent through a session reaches the server on the same socket,
 * which is what MULTI/EXEC, WATCH and SUBSCRIBE require. The session also
 * mirrors the connection-level state (MULTI, watched keys, subscriptions)
 * from the server's replies so the UI can render it.
 */
export class BridgeSession {
  readonly id: string;
  readonly target: Target;
  readonly kind: SessionKind;
  private connection: RespConnection | null = null;
  private inMulti = false;
  private watching = new Set<string>();

  constructor(id: string, target: Target, kind: SessionKind) {
    this.id = id;
    this.target = target;
    this.kind = kind;
  }

  get address(): string {
    return this.connection?.address ?? "";
  }

  get state(): SessionState {
    return {
      subscriptions: this.connection?.subscriptions.slice() ?? [],
      inMulti: this.inMulti,
      watching: [...this.watching],
    };
  }

  async open(onPush: (push: PushEvent) => void, onClose: (reason: string) => void): Promise<void> {
    const endpoint = getEndpoint(this.target);
    if (endpoint === null) throw new Error(`${this.target} is not configured`);

    const connection = new RespConnection({
      host: endpoint.host,
      port: endpoint.port,
      connectTimeoutMs: CONNECT_TIMEOUT_MS,
      commandTimeoutMs: COMMAND_TIMEOUT_MS,
    });

    connection.onPush((frame) => {
      const push: PushEvent = {
        id: randomUUID(),
        receivedAt: Date.now(),
        raw: frame.raw.toString("utf8"),
        value: frame.value,
      };

      if (isMessagePush(frame.value) && frame.value.type === "array") {
        push.message = { channel: bulk(frame.value.value[1]), payload: bulk(frame.value.value[2]) };
      }

      onPush(push);
    });

    connection.onClose((error) => {
      if (error) onClose(error.message);
    });

    await connection.connect();
    this.connection = connection;
  }

  async execute(args: string[]): Promise<CommandResponse> {
    const context = { target: this.target, via: "session" as const, sessionId: this.id };
    const invalid = validateArgs(args);
    if (invalid) return failure(context, Array.isArray(args) ? args : [], "bad-request", invalid);

    const name = args[0].toUpperCase();

    if (FORBIDDEN_COMMANDS.has(name)) {
      return failure(context, args, "bad-request", `${name} would turn this session into a replication stream`);
    }

    if (this.connection === null || this.connection.state === "closed") {
      return failure(context, args, "connection", "session is not connected");
    }

    const response = await runOn(this.connection, context, args);
    this.track(name, args, response);
    return response;
  }

  private track(name: string, args: string[], response: CommandResponse): void {
    if (response.outcome === "failure") return;

    const ok = response.outcome === "reply";

    switch (name) {
      case "MULTI":
        if (ok) this.inMulti = true;
        break;
      case "EXEC":
      case "DISCARD":
        // Both end the transaction and drop watches, including EXEC's null
        // reply on a WATCH conflict. "EXEC without MULTI" changes nothing.
        if (ok) {
          this.inMulti = false;
          this.watching.clear();
        }
        break;
      case "WATCH":
        if (ok) args.slice(1).forEach((key) => this.watching.add(key));
        break;
      case "UNWATCH":
        if (ok) this.watching.clear();
        break;
    }
  }

  close(): void {
    this.connection?.close();
    this.connection = null;
  }
}
