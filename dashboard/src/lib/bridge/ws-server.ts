import type { IncomingMessage } from "node:http";
import type { Duplex } from "node:stream";

import { WebSocketServer, type RawData, type WebSocket } from "ws";

import { isTarget } from "../resp/types";
import { BridgeSession } from "./session";
import type { ClientMessage, ServerMessage, SessionKind } from "./ws-protocol";

export const WS_PATH = "/ws";

const MAX_SESSIONS_PER_SOCKET = 16;
const MAX_MESSAGE_BYTES = 9 * 1024 * 1024;
const HEARTBEAT_MS = 30_000;
const SESSION_KINDS: readonly SessionKind[] = ["cli", "transaction", "subscriber"];

function parseMessage(data: RawData): ClientMessage | null {
  let parsed: unknown;

  try {
    parsed = JSON.parse(data.toString());
  } catch {
    return null;
  }

  if (typeof parsed !== "object" || parsed === null) return null;

  const message = parsed as Record<string, unknown>;
  if (typeof message.sessionId !== "string" || message.sessionId.length > 64) return null;

  switch (message.type) {
    case "session.open":
      return isTarget(message.target) && SESSION_KINDS.includes(message.kind as SessionKind)
        ? (message as unknown as ClientMessage)
        : null;
    case "session.command":
      return typeof message.requestId === "string" && Array.isArray(message.args)
        ? (message as unknown as ClientMessage)
        : null;
    case "session.close":
      return message as unknown as ClientMessage;
    default:
      return null;
  }
}

/**
 * Bridges one browser WebSocket to any number of dedicated TCP sessions.
 * Closing the WebSocket closes every session it opened.
 */
function handleConnection(ws: WebSocket): void {
  const sessions = new Map<string, BridgeSession>();
  let alive = true;

  const send = (message: ServerMessage) => {
    if (ws.readyState === ws.OPEN) ws.send(JSON.stringify(message));
  };

  const heartbeat = setInterval(() => {
    if (!alive) {
      ws.terminate();
      return;
    }
    alive = false;
    ws.ping();
  }, HEARTBEAT_MS);

  ws.on("pong", () => {
    alive = true;
  });

  const closeSession = (sessionId: string, reason: string, notify: boolean) => {
    const session = sessions.get(sessionId);
    if (!session) return;
    sessions.delete(sessionId);
    session.close();
    if (notify) send({ type: "session.closed", sessionId, reason });
  };

  ws.on("message", async (data) => {
    const message = parseMessage(data);
    if (message === null) return;

    switch (message.type) {
      case "session.open": {
        if (sessions.has(message.sessionId)) return;

        if (sessions.size >= MAX_SESSIONS_PER_SOCKET) {
          send({ type: "session.error", sessionId: message.sessionId, message: "too many open sessions" });
          return;
        }

        const session = new BridgeSession(message.sessionId, message.target, message.kind);
        sessions.set(message.sessionId, session);

        try {
          await session.open(
            (push) => send({ type: "session.push", sessionId: session.id, push, state: session.state }),
            (reason) => {
              if (sessions.get(session.id) === session) closeSession(session.id, reason, true);
            },
          );
        } catch (error) {
          sessions.delete(message.sessionId);
          send({
            type: "session.error",
            sessionId: message.sessionId,
            message: error instanceof Error ? error.message : String(error),
          });
          return;
        }

        if (sessions.get(session.id) !== session) {
          session.close();
          return;
        }

        send({
          type: "session.opened",
          sessionId: session.id,
          target: session.target,
          address: session.address,
          state: session.state,
        });
        return;
      }

      case "session.command": {
        const session = sessions.get(message.sessionId);

        if (!session) {
          send({ type: "session.closed", sessionId: message.sessionId, reason: "unknown session" });
          return;
        }

        const response = await session.execute(message.args);
        send({
          type: "session.result",
          sessionId: session.id,
          requestId: message.requestId,
          response,
          state: session.state,
        });
        return;
      }

      case "session.close":
        closeSession(message.sessionId, "closed by client", false);
        return;
    }
  });

  ws.on("close", () => {
    clearInterval(heartbeat);
    for (const id of [...sessions.keys()]) closeSession(id, "websocket closed", false);
  });

  ws.on("error", () => ws.terminate());
}

export function createBridgeWebSocketServer(): {
  handleUpgrade: (request: IncomingMessage, socket: Duplex, head: Buffer) => void;
  close: () => void;
} {
  const wss = new WebSocketServer({ noServer: true, maxPayload: MAX_MESSAGE_BYTES });
  wss.on("connection", handleConnection);

  return {
    handleUpgrade(request, socket, head) {
      wss.handleUpgrade(request, socket, head, (ws) => wss.emit("connection", ws, request));
    },
    close() {
      for (const client of wss.clients) client.terminate();
      wss.close();
    },
  };
}
