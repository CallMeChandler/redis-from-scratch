"use client";

import { createContext, useCallback, useContext, useMemo, useState, type ReactNode } from "react";

import type { CommandResponse } from "@/lib/resp/types";

export interface ActivityEntry {
  response: CommandResponse;
  /** Which page/panel issued it, e.g. "Strings" or "CLI". */
  origin: string;
  /** Issued automatically (refreshes, snapshots) rather than by a click. */
  auto: boolean;
}

interface ActivityContextValue {
  entries: ActivityEntry[];
  record: (response: CommandResponse, origin: string, auto?: boolean) => void;
  clear: () => void;
  inspected: CommandResponse | null;
  inspect: (response: CommandResponse | null) => void;
}

const MAX_ENTRIES = 300;

const ActivityContext = createContext<ActivityContextValue | null>(null);

/** Client-side command log + global inspector selection. Nothing is persisted. */
export function ActivityProvider({ children }: { children: ReactNode }) {
  const [entries, setEntries] = useState<ActivityEntry[]>([]);
  const [inspected, setInspected] = useState<CommandResponse | null>(null);

  const record = useCallback((response: CommandResponse, origin: string, auto = false) => {
    setEntries((current) => [{ response, origin, auto }, ...current].slice(0, MAX_ENTRIES));
  }, []);

  const clear = useCallback(() => setEntries([]), []);

  const value = useMemo(
    () => ({ entries, record, clear, inspected, inspect: setInspected }),
    [entries, record, clear, inspected],
  );

  return <ActivityContext.Provider value={value}>{children}</ActivityContext.Provider>;
}

export function useActivity(): ActivityContextValue {
  const context = useContext(ActivityContext);
  if (!context) throw new Error("useActivity must be used inside ActivityProvider");
  return context;
}
