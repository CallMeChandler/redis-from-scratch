"use client";

import { useCallback, useState } from "react";

import { useActivity } from "@/components/providers/activity-provider";
import { useToast } from "@/components/providers/toast-provider";
import { postCommand } from "@/lib/client/api";
import { displayCommand } from "@/lib/resp/format";
import type { CommandResponse, Target } from "@/lib/resp/types";

export interface RunOptions {
  /** Automatic/background command: logged as auto, no failure toast. */
  auto?: boolean;
}

/**
 * Runs stateless commands through /api/command, records them in the
 * activity log and toasts on network failures. Redis error replies are
 * normal results and are left to the caller to display.
 */
export function useCommandRunner(origin: string) {
  const { record } = useActivity();
  const notify = useToast();
  const [pending, setPending] = useState(0);

  const run = useCallback(
    async (args: string[], target: Target = "primary", options: RunOptions = {}): Promise<CommandResponse> => {
      if (!options.auto) setPending((count) => count + 1);
      const response = await postCommand(args, target);
      if (!options.auto) setPending((count) => count - 1);

      record(response, origin, options.auto);

      if (response.outcome === "failure" && !options.auto) {
        notify({ tone: "error", title: `${displayCommand(args)} failed`, description: response.error });
      }

      return response;
    },
    [notify, origin, record],
  );

  return { run, pending: pending > 0 };
}
