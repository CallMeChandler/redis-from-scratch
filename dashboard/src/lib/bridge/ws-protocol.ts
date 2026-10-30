import type { CommandResponse, RespValue, Target } from "../resp/types";

/**
 * Messages exchanged over the single dashboard WebSocket (/ws).
 * One browser tab multiplexes any number of sessions over it; each
 * session owns one dedicated TCP connection to the C++ server.
 */

export type SessionKind = "cli" | "transaction" | "subscriber";

export interface SessionState {
  /** Channels confirmed by subscribe/unsubscribe replies. */
  subscriptions: string[];
  /** True between MULTI's +OK and EXEC/DISCARD. */
  inMulti: boolean;
  /** Keys watched on this connection; cleared by EXEC, DISCARD, UNWATCH. */
  watching: string[];
}

export type ClientMessage =
  | { type: "session.open"; sessionId: string; target: Target; kind: SessionKind }
  | { type: "session.command"; sessionId: string; requestId: string; args: string[] }
  | { type: "session.close"; sessionId: string };

export interface PushEvent {
  id: string;
  receivedAt: number;
  raw: string;
  value: RespValue;
  /** Set for `["message", channel, payload]` pushes. */
  message?: { channel: string; payload: string };
}

export type ServerMessage =
  | {
      type: "session.opened";
      sessionId: string;
      target: Target;
      address: string;
      state: SessionState;
    }
  | { type: "session.error"; sessionId: string; message: string }
  | { type: "session.closed"; sessionId: string; reason: string }
  | {
      type: "session.result";
      sessionId: string;
      requestId: string;
      response: CommandResponse;
      state: SessionState;
    }
  | { type: "session.push"; sessionId: string; push: PushEvent; state: SessionState };
