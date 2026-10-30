"use client";

import { useCallback, useEffect, useState } from "react";

import { useActivity } from "@/components/providers/activity-provider";
import { useHealth } from "@/components/providers/health-provider";
import { fetchInfo } from "@/lib/client/api";
import { flattenInfo } from "@/lib/info";
import type { InfoResponse, Target } from "@/lib/resp/types";

/**
 * Loads INFO (optionally one section) and keeps it fresh.
 * intervalMs = 0 disables auto refresh; the top-bar refresh always reloads.
 */
export function useInfo(target: Target, section: string | undefined, intervalMs: number, origin: string) {
  const { record } = useActivity();
  const { refreshToken } = useHealth();
  const [state, setState] = useState<{ data: InfoResponse | null; previous: InfoResponse | null; loadedAt: number | null }>({
    data: null,
    previous: null,
    loadedAt: null,
  });

  const load = useCallback(async () => {
    const result = await fetchInfo(target, section);
    record(result.ok ? result.execution : result.response, origin, true);
    setState((current) => ({
      // Keep the previous sample only while it is from the same target.
      previous: current.data?.target === result.target ? current.data : null,
      data: result,
      loadedAt: Date.now(),
    }));
  }, [target, section, origin, record]);

  useEffect(() => {
    let cancelled = false;
    let timer: ReturnType<typeof setTimeout> | undefined;
    let first = true;

    const tick = async () => {
      // Always load once; afterwards skip polling while the tab is hidden.
      if (first || document.visibilityState === "visible") await load();
      first = false;
      if (!cancelled && intervalMs > 0) timer = setTimeout(tick, intervalMs);
    };

    timer = setTimeout(tick, 0);
    return () => {
      cancelled = true;
      clearTimeout(timer);
    };
  }, [load, intervalMs, refreshToken]);

  const { data, previous, loadedAt } = state;
  const fields = data?.ok ? flattenInfo(data.sections) : {};
  const previousFields = previous?.ok ? flattenInfo(previous.sections) : null;

  return { data, fields, previousFields, loadedAt, reload: load };
}
