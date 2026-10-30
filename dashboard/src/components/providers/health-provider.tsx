"use client";

import { createContext, useCallback, useContext, useEffect, useMemo, useState, type ReactNode } from "react";

import { fetchHealth } from "@/lib/client/api";
import type { HealthResponse } from "@/lib/resp/types";

interface HealthContextValue {
  health: HealthResponse | null;
  checking: boolean;
  /** Re-checks health and bumps refreshToken so pages can reload their data. */
  refresh: () => void;
  refreshToken: number;
}

const HealthContext = createContext<HealthContextValue | null>(null);

const POLL_MS = 5000;

export function HealthProvider({ children }: { children: ReactNode }) {
  const [health, setHealth] = useState<HealthResponse | null>(null);
  const [checking, setChecking] = useState(false);
  const [refreshToken, setRefreshToken] = useState(0);

  const check = useCallback(async () => {
    setChecking(true);
    const result = await fetchHealth();
    setHealth(result);
    setChecking(false);
  }, []);

  useEffect(() => {
    let timer: ReturnType<typeof setTimeout>;
    let cancelled = false;

    const loop = async () => {
      if (document.visibilityState === "visible") await check();
      if (!cancelled) timer = setTimeout(loop, POLL_MS);
    };

    timer = setTimeout(loop, 0);
    return () => {
      cancelled = true;
      clearTimeout(timer);
    };
  }, [check]);

  const refresh = useCallback(() => {
    void check();
    setRefreshToken((token) => token + 1);
  }, [check]);

  const value = useMemo(() => ({ health, checking, refresh, refreshToken }), [health, checking, refresh, refreshToken]);

  return <HealthContext.Provider value={value}>{children}</HealthContext.Provider>;
}

export function useHealth(): HealthContextValue {
  const context = useContext(HealthContext);
  if (!context) throw new Error("useHealth must be used inside HealthProvider");
  return context;
}
