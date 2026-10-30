import type { ClientMessage, PushEvent, ServerMessage, SessionKind, SessionState } from "../bridge/ws-protocol";
import type { CommandResponse, Target } from "../resp/types";
import { makeId } from "./id";

export type SocketStatus = "connecting" | "open" | "closed";
export type SessionStatus = "connecting" | "open" | "closed" | "error";

export const EMPTY_SESSION_STATE: SessionState = { subscriptions: [], inMulti: false, watching: [] };

export interface SessionSnapshot {
  status: SessionStatus;
  address: string;
  error: string | null;
  state: SessionState;
}

type Listener = () => void;

/**
 * Browser half of a bridge session: a handle to one dedicated TCP socket
 * held by the Node server. All commands sent through it share that socket.
 */
export class ClientSession {
  readonly id = makeId();
  private snapshot: SessionSnapshot = { status: "connecting", address: "", error: null, state: EMPTY_SESSION_STATE };
  private readonly listeners = new Set<Listener>();
  private readonly pushListeners = new Set<(push: PushEvent) => void>();
  private readonly pending = new Map<string, { args: string[]; resolve: (response: CommandResponse) => void }>();

  constructor(
    private readonly socket: BridgeSocket,
    readonly target: Target,
    readonly kind: SessionKind,
  ) {}

  getSnapshot = (): SessionSnapshot => this.snapshot;

  subscribe = (listener: Listener): (() => void) => {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  };

  onPush(listener: (push: PushEvent) => void): () => void {
    this.pushListeners.add(listener);
    return () => this.pushListeners.delete(listener);
  }

  /** Sends a command on this session's socket; resolves with the reply (never rejects). */
  execute(args: string[]): Promise<CommandResponse> {
    if (this.snapshot.status !== "open") {
      return Promise.resolve(this.localFailure(args, "session is not open"));
    }

    const requestId = makeId();
    return new Promise((resolve) => {
      this.pending.set(requestId, { args, resolve });
      this.socket.send({ type: "session.command", sessionId: this.id, requestId, args });
    });
  }

  close(): void {
    if (this.snapshot.status === "closed") return;
    this.socket.send({ type: "session.close", sessionId: this.id });
    this.finish("closed", null);
  }

  /** @internal */
  handle(message: ServerMessage): void {
    switch (message.type) {
      case "session.opened":
        this.update({ status: "open", address: message.address, error: null, state: message.state });
        return;
      case "session.error":
        this.finish("error", message.message);
        return;
      case "session.closed":
        this.finish("closed", message.reason);
        return;
      case "session.result": {
        const request = this.pending.get(message.requestId);
        this.pending.delete(message.requestId);
        this.update({ ...this.snapshot, state: message.state });
        request?.resolve(message.response);
        return;
      }
      case "session.push":
        this.update({ ...this.snapshot, state: message.state });
        for (const listener of this.pushListeners) listener(message.push);
        return;
    }
  }

  /** @internal Called when the WebSocket itself drops. */
  lost(reason: string): void {
    this.finish("closed", reason);
  }

  private finish(status: "closed" | "error", reason: string | null): void {
    this.socket.forget(this.id);
    this.update({ ...this.snapshot, status, error: reason, state: EMPTY_SESSION_STATE });

    for (const { args, resolve } of this.pending.values()) {
      resolve(this.localFailure(args, reason ?? "session closed"));
    }
    this.pending.clear();
  }

  private localFailure(args: string[], error: string): CommandResponse {
    return {
      ok: false,
      outcome: "failure",
      id: makeId(),
      kind: "connection",
      error,
      target: this.target,
      via: "session",
      sessionId: this.id,
      command: args,
      startedAt: Date.now(),
    };
  }

  private update(next: SessionSnapshot): void {
    this.snapshot = next;
    for (const listener of this.listeners) listener();
  }
}

/** One WebSocket per tab, multiplexing every session; reconnects with backoff. */
export class BridgeSocket {
  private ws: WebSocket | null = null;
  private status: SocketStatus = "closed";
  private readonly sessions = new Map<string, ClientSession>();
  private readonly outbox: ClientMessage[] = [];
  private readonly listeners = new Set<Listener>();
  private retry = 0;
  private retryTimer: ReturnType<typeof setTimeout> | null = null;

  getStatus = (): SocketStatus => this.status;

  subscribe = (listener: Listener): (() => void) => {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  };

  openSession(target: Target, kind: SessionKind): ClientSession {
    const session = new ClientSession(this, target, kind);
    this.sessions.set(session.id, session);
    this.send({ type: "session.open", sessionId: session.id, target, kind });
    return session;
  }

  /** @internal */
  forget(sessionId: string): void {
    this.sessions.delete(sessionId);
  }

  /** @internal */
  send(message: ClientMessage): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(message));
      return;
    }
    this.outbox.push(message);
    this.connect();
  }

  private setStatus(status: SocketStatus): void {
    this.status = status;
    for (const listener of this.listeners) listener();
  }

  private connect(): void {
    if (this.ws || this.retryTimer) return;

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const ws = new WebSocket(`${protocol}//${window.location.host}/ws`);
    this.ws = ws;
    this.setStatus("connecting");

    ws.onopen = () => {
      this.retry = 0;
      this.setStatus("open");
      for (const message of this.outbox.splice(0)) ws.send(JSON.stringify(message));
    };

    ws.onmessage = (event: MessageEvent<string>) => {
      let message: ServerMessage;
      try {
        message = JSON.parse(event.data) as ServerMessage;
      } catch {
        return;
      }
      this.sessions.get(message.sessionId)?.handle(message);
    };

    ws.onclose = () => {
      this.ws = null;
      this.outbox.length = 0;
      this.setStatus("closed");
      for (const session of [...this.sessions.values()]) session.lost("dashboard WebSocket disconnected");
      this.sessions.clear();

      // Reconnect lazily: the next openSession/send triggers connect(),
      // after a short backoff so a dead backend isn't hammered.
      const delay = Math.min(10_000, 500 * 2 ** this.retry);
      this.retry += 1;
      this.retryTimer = setTimeout(() => {
        this.retryTimer = null;
        if (this.outbox.length > 0) this.connect();
      }, delay);
    };
  }
}

let shared: BridgeSocket | null = null;

export function getBridgeSocket(): BridgeSocket {
  shared ??= new BridgeSocket();
  return shared;
}
