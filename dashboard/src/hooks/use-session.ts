"use client";

import { useCallback, useEffect, useEffectEvent, useState, useSyncExternalStore } from "react";

import { useActivity } from "@/components/providers/activity-provider";
import type { PushEvent, SessionKind } from "@/lib/bridge/ws-protocol";
import {
  EMPTY_SESSION_STATE,
  getBridgeSocket,
  type ClientSession,
  type SessionSnapshot,
  type SocketStatus,
} from "@/lib/client/bridge-socket";
import { makeId } from "@/lib/client/id";
import type { CommandResponse, Target } from "@/lib/resp/types";

const IDLE: SessionSnapshot = { status: "closed", address: "", error: null, state: EMPTY_SESSION_STATE };

/** Holds at most one live ClientSession and re-publishes its snapshot. */
class SessionSlot {
  private session: ClientSession | null = null;
  private unsubscribe: (() => void) | null = null;
  private snapshot: SessionSnapshot = IDLE;
  private readonly listeners = new Set<() => void>();
  private readonly pushListeners = new Set<(push: PushEvent) => void>();

  getSnapshot = () => this.snapshot;
  getServerSnapshot = () => IDLE;

  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  };

  get sessionId(): string | null {
    return this.session?.id ?? null;
  }

  onPush(listener: (push: PushEvent) => void): () => void {
    this.pushListeners.add(listener);
    return () => this.pushListeners.delete(listener);
  }

  open(target: Target, kind: SessionKind): void {
    this.close();
    const session = getBridgeSocket().openSession(target, kind);
    const offState = session.subscribe(() => this.publish(session.getSnapshot()));
    const offPush = session.onPush((push) => this.pushListeners.forEach((listener) => listener(push)));
    this.session = session;
    this.unsubscribe = () => {
      offState();
      offPush();
    };
    this.publish(session.getSnapshot());
  }

  close(): void {
    this.unsubscribe?.();
    this.session?.close();
    this.session = null;
    this.unsubscribe = null;
    this.publish(IDLE);
  }

  execute(args: string[]): Promise<CommandResponse> | null {
    return this.session?.execute(args) ?? null;
  }

  private publish(snapshot: SessionSnapshot): void {
    this.snapshot = snapshot;
    this.listeners.forEach((listener) => listener());
  }
}

interface UseSessionOptions {
  target: Target;
  kind: SessionKind;
  /** Activity log label. */
  origin: string;
  /** Open as soon as the component mounts (default true). */
  autoOpen?: boolean;
  onPush?: (push: PushEvent) => void;
}

/**
 * A dedicated server-side TCP connection for this component, kept open
 * while mounted. Changing target/kind replaces the connection.
 */
export function useSession({ target, kind, origin, autoOpen = true, onPush }: UseSessionOptions) {
  const [slot] = useState(() => new SessionSlot());
  const { record } = useActivity();
  const snapshot = useSyncExternalStore(slot.subscribe, slot.getSnapshot, slot.getServerSnapshot);

  useEffect(() => {
    if (autoOpen) slot.open(target, kind);
    return () => slot.close();
  }, [slot, target, kind, autoOpen]);

  const handlePush = useEffectEvent((push: PushEvent) => onPush?.(push));
  useEffect(() => slot.onPush((push) => handlePush(push)), [slot]);

  const execute = useCallback(
    async (args: string[]): Promise<CommandResponse> => {
      const pending = slot.execute(args);
      const response: CommandResponse = pending
        ? await pending
        : {
            ok: false,
            outcome: "failure",
            id: makeId(),
            kind: "connection",
            error: "session is not open",
            target,
            via: "session",
            command: args,
            startedAt: Date.now(),
          };
      record(response, origin);
      return response;
    },
    [slot, record, origin, target],
  );

  const reopen = useCallback(() => slot.open(target, kind), [slot, target, kind]);
  const close = useCallback(() => slot.close(), [slot]);

  return { ...snapshot, execute, reopen, close };
}

const subscribeSocket = (listener: () => void) => getBridgeSocket().subscribe(listener);
const socketStatus = (): SocketStatus => getBridgeSocket().getStatus();
const serverSocketStatus = (): SocketStatus => "closed";

export function useBridgeStatus(): SocketStatus {
  return useSyncExternalStore(subscribeSocket, socketStatus, serverSocketStatus);
}
