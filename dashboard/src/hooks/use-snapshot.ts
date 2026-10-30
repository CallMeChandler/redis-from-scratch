"use client";

import { useEffect, useState } from "react";

import { useHealth } from "@/components/providers/health-provider";
import type { CommandResponse, Target } from "@/lib/resp/types";

import { useCommandRunner } from "./use-command";

/**
 * Re-runs a fixed set of read commands whenever `version` changes (i.e.
 * after every command on the page), so a panel can show the live state of
 * the key being experimented on. Runs are logged as "auto".
 */
export function useSnapshot(origin: string, commands: string[][] | null, target: Target, version: number) {
  const { run } = useCommandRunner(origin);
  const { refreshToken } = useHealth();
  const [responses, setResponses] = useState<CommandResponse[] | null>(null);
  const signature = commands ? JSON.stringify(commands) : null;

  useEffect(() => {
    if (signature === null) return;
    let cancelled = false;
    const list = JSON.parse(signature) as string[][];

    void Promise.all(list.map((args) => run(args, target, { auto: true }))).then((results) => {
      if (!cancelled) setResponses(results);
    });

    return () => {
      cancelled = true;
    };
  }, [signature, target, version, run, refreshToken]);

  return signature === null ? null : responses;
}
