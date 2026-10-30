import { Buffer } from "node:buffer";
import net from "node:net";

import { encodeCommand } from "./encoder";
import { RespParser, RespProtocolError, type ParsedFrame } from "./parser";
import type { RespValue } from "./types";

export type ConnectionErrorKind = "connection" | "timeout" | "protocol";

export class RespConnectionError extends Error {
  constructor(
    readonly kind: ConnectionErrorKind,
    message: string,
  ) {
    super(message);
    this.name = "RespConnectionError";
  }
}

export interface RoundTrip {
  rawRequest: string;
  rawResponse: string;
  replies: RespValue[];
  latencyMs: number;
  startedAt: number;
}

export interface RespConnectionOptions {
  host: string;
  port: number;
  connectTimeoutMs?: number;
  commandTimeoutMs?: number;
}

export type ConnectionState = "idle" | "connecting" | "open" | "closed";

interface PendingCommand {
  args: readonly string[];
  request: Buffer;
  expected: number;
  frames: ParsedFrame[];
  startedAt: number;
  startedHr: bigint;
  timer?: NodeJS.Timeout;
  resolve: (result: RoundTrip) => void;
  reject: (error: RespConnectionError) => void;
}

type PushListener = (frame: ParsedFrame) => void;
type CloseListener = (error: RespConnectionError | null) => void;

function bulk(value: RespValue | undefined): string | null {
  return value?.type === "bulk-string" ? value.value : null;
}

/** `["message", channel, payload]` frames are server pushes, not replies. */
export function isMessagePush(value: RespValue): boolean {
  return value.type === "array" && value.value.length === 3 && bulk(value.value[0]) === "message";
}

/**
 * A single RESP2 TCP connection to the server.
 *
 * Commands are written one batch at a time, and replies are matched to
 * commands in FIFO order. A command can produce several reply frames
 * (SUBSCRIBE a b c -> three confirmations), so the number of frames to
 * wait for is computed when the command is written, from the subscription
 * state the connection has observed so far.
 */
export class RespConnection {
  private socket: net.Socket | null = null;
  private readonly parser = new RespParser();
  private readonly queue: PendingCommand[] = [];
  private inFlight: PendingCommand[] = [];
  private readonly pushListeners = new Set<PushListener>();
  private readonly closeListeners = new Set<CloseListener>();
  private readonly channels = new Set<string>();
  private connectPromise: Promise<void> | null = null;
  private drainScheduled = false;
  private currentState: ConnectionState = "idle";

  readonly host: string;
  readonly port: number;
  private readonly connectTimeoutMs: number;
  private readonly commandTimeoutMs: number;

  constructor(options: RespConnectionOptions) {
    this.host = options.host;
    this.port = options.port;
    this.connectTimeoutMs = options.connectTimeoutMs ?? 2000;
    this.commandTimeoutMs = options.commandTimeoutMs ?? 5000;
  }

  get state(): ConnectionState {
    return this.currentState;
  }

  /** Channels this connection is subscribed to, as confirmed by the server. */
  get subscriptions(): readonly string[] {
    return [...this.channels];
  }

  connect(): Promise<void> {
    if (this.currentState === "open") return Promise.resolve();
    if (this.connectPromise) return this.connectPromise;
    if (this.currentState === "closed") {
      return Promise.reject(new RespConnectionError("connection", "connection is closed"));
    }

    this.currentState = "connecting";

    this.connectPromise = new Promise<void>((resolve, reject) => {
      const socket = net.createConnection({ host: this.host, port: this.port });
      this.socket = socket;

      const timer = setTimeout(() => {
        socket.destroy();
        fail(new RespConnectionError("timeout", `connect to ${this.address} timed out after ${this.connectTimeoutMs} ms`));
      }, this.connectTimeoutMs);

      const fail = (error: RespConnectionError) => {
        clearTimeout(timer);
        this.connectPromise = null;
        this.teardown(error);
        reject(error);
      };

      socket.once("connect", () => {
        clearTimeout(timer);
        socket.setNoDelay(true);
        socket.setKeepAlive(true, 30_000);
        socket.off("error", onConnectError);
        socket.on("error", (error) => this.teardown(new RespConnectionError("connection", error.message)));
        socket.on("close", () => this.teardown(new RespConnectionError("connection", "connection closed by server")));
        socket.on("data", (chunk: Buffer) => this.onData(chunk));
        this.currentState = "open";
        this.connectPromise = null;
        resolve();
        this.drain();
      });

      const onConnectError = (error: NodeJS.ErrnoException) => {
        const reason = error.code === "ECONNREFUSED" ? "connection refused" : error.message;
        fail(new RespConnectionError("connection", `${this.address}: ${reason}`));
      };

      socket.once("error", onConnectError);
    });

    return this.connectPromise;
  }

  get address(): string {
    return `${this.host}:${this.port}`;
  }

  execute(args: readonly string[]): Promise<RoundTrip> {
    return new Promise<RoundTrip>((resolve, reject) => {
      if (this.currentState === "closed") {
        reject(new RespConnectionError("connection", "connection is closed"));
        return;
      }

      this.queue.push({
        args,
        request: encodeCommand(args),
        expected: 1,
        frames: [],
        startedAt: 0,
        startedHr: BigInt(0),
        resolve,
        reject,
      });
      this.scheduleDrain();
    });
  }

  /**
   * Writes several commands in a single TCP write and waits for all replies.
   * Useful to verify the server handles pipelining and that the parser can
   * split multiple replies arriving in one chunk.
   */
  pipeline(commands: readonly (readonly string[])[]): Promise<RoundTrip[]> {
    return Promise.all(commands.map((args) => this.execute(args)));
  }

  onPush(listener: PushListener): () => void {
    this.pushListeners.add(listener);
    return () => this.pushListeners.delete(listener);
  }

  onClose(listener: CloseListener): () => void {
    this.closeListeners.add(listener);
    return () => this.closeListeners.delete(listener);
  }

  close(): void {
    this.teardown(null);
  }

  private expectedReplies(args: readonly string[]): number {
    const name = args[0]?.toUpperCase();

    if (name === "SUBSCRIBE") {
      return Math.max(1, args.length - 1);
    }

    if (name === "UNSUBSCRIBE") {
      return args.length > 1 ? args.length - 1 : Math.max(1, this.channels.size);
    }

    return 1;
  }

  /** Defers writing to a microtask so synchronous execute() calls share one write. */
  private scheduleDrain(): void {
    if (this.drainScheduled) return;
    this.drainScheduled = true;
    queueMicrotask(() => {
      this.drainScheduled = false;
      this.drain();
    });
  }

  /**
   * Writes everything queued so far as one batch, but only once the previous
   * batch has been fully answered. Batching keeps pipelines in one write,
   * while waiting between batches keeps multi-frame reply counting exact.
   */
  private drain(): void {
    if (this.currentState !== "open" || this.socket === null) return;
    if (this.inFlight.length > 0 || this.queue.length === 0) return;

    const batch = this.queue.splice(0, this.queue.length);
    const startedAt = Date.now();
    const startedHr = process.hrtime.bigint();

    for (const command of batch) {
      command.expected = this.expectedReplies(command.args);
      command.startedAt = startedAt;
      command.startedHr = startedHr;
      command.timer = setTimeout(() => {
        this.socket?.destroy();
        this.teardown(
          new RespConnectionError(
            "timeout",
            `no reply to ${command.args[0] ?? "command"} within ${this.commandTimeoutMs} ms`,
          ),
        );
      }, this.commandTimeoutMs);
    }

    this.inFlight = batch;
    this.socket.write(Buffer.concat(batch.map((command) => command.request)));
  }

  private onData(chunk: Buffer): void {
    let frames: ParsedFrame[];

    try {
      frames = this.parser.push(chunk);
    } catch (error) {
      const message = error instanceof RespProtocolError ? error.message : String(error);
      this.socket?.destroy();
      this.teardown(new RespConnectionError("protocol", `malformed RESP from server: ${message}`));
      return;
    }

    for (const frame of frames) {
      this.onFrame(frame);
    }

    if (this.inFlight.length === 0) {
      this.drain();
    }
  }

  private trackSubscriptions(value: RespValue): void {
    if (value.type !== "array" || value.value.length !== 3) return;

    const kind = bulk(value.value[0]);
    const channel = bulk(value.value[1]);

    if (kind === "subscribe" && channel !== null) this.channels.add(channel);
    if (kind === "unsubscribe" && channel !== null) this.channels.delete(channel);
  }

  private onFrame(frame: ParsedFrame): void {
    const head = this.inFlight[0];

    if (head === undefined || (this.channels.size > 0 && isMessagePush(frame.value))) {
      for (const listener of this.pushListeners) listener(frame);
      return;
    }

    this.trackSubscriptions(frame.value);
    head.frames.push(frame);

    if (frame.value.type !== "error" && head.frames.length < head.expected) {
      return;
    }

    this.inFlight.shift();
    clearTimeout(head.timer);

    head.resolve({
      rawRequest: head.request.toString("utf8"),
      rawResponse: Buffer.concat(head.frames.map((item) => item.raw)).toString("utf8"),
      replies: head.frames.map((item) => item.value),
      latencyMs: Number(process.hrtime.bigint() - head.startedHr) / 1e6,
      startedAt: head.startedAt,
    });
  }

  private teardown(error: RespConnectionError | null): void {
    if (this.currentState === "closed") return;

    this.currentState = "closed";
    this.socket?.removeAllListeners("data");
    this.socket?.destroy();
    this.socket = null;
    this.channels.clear();
    this.parser.reset();

    const failure = error ?? new RespConnectionError("connection", "connection closed");

    for (const command of [...this.inFlight, ...this.queue]) {
      clearTimeout(command.timer);
      command.reject(failure);
    }

    this.inFlight = [];
    this.queue.length = 0;

    for (const listener of this.closeListeners) listener(error);
  }
}
