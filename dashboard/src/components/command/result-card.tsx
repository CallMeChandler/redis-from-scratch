"use client";

import { Code2, ScanSearch, Terminal } from "lucide-react";
import { useState, type ReactNode } from "react";

import { useActivity } from "@/components/providers/activity-provider";
import { Card } from "@/components/ui/card";
import { EmptyState } from "@/components/ui/empty-state";
import { cn } from "@/lib/cn";
import { formatLatency } from "@/lib/resp/format";
import type { CommandResponse, CommandRoundTrip } from "@/lib/resp/types";

import { CommandLine } from "./command-line";
import { OutcomeBadge } from "./outcome-badge";
import { RespRaw } from "./resp-raw";
import { RespValueView, TypeTag } from "./resp-value-view";

export interface ResultCardProps {
  response: CommandResponse | null;
  title?: string;
  /** Optional richer rendering for successful replies (chips, bars, lists). */
  visualize?: (response: CommandRoundTrip) => ReactNode;
  /** Treat Redis errors as the expected outcome (e.g. READONLY demo). */
  expectError?: boolean;
  emptyText?: string;
  className?: string;
}

/** Reusable response viewer: command, outcome, latency, value, optional RESP. */
export function ResultCard({ response, title = "Response", visualize, expectError, emptyText, className }: ResultCardProps) {
  const { inspect } = useActivity();
  const [showResp, setShowResp] = useState(false);

  return (
    <Card className={cn("overflow-hidden", className)}>
      <header className="flex items-center justify-between gap-2 border-b border-border px-4 py-2.5">
        <h2 className="text-[11px] font-medium tracking-wide text-faint uppercase">{title}</h2>
        {response && (
          <div className="flex items-center gap-1">
            <button
              type="button"
              onClick={() => setShowResp(!showResp)}
              className={cn(
                "flex items-center gap-1 rounded px-1.5 py-1 text-[11px] font-medium transition-colors hover:bg-hover",
                showResp ? "text-accent" : "text-muted",
              )}
              aria-pressed={showResp}
            >
              <Code2 className="size-3.5" /> RESP
            </button>
            <button
              type="button"
              onClick={() => inspect(response)}
              className="flex items-center gap-1 rounded px-1.5 py-1 text-[11px] font-medium text-muted transition-colors hover:bg-hover hover:text-text"
            >
              <ScanSearch className="size-3.5" /> Inspect
            </button>
          </div>
        )}
      </header>

      {response === null ? (
        <EmptyState icon={<Terminal className="size-5" />} title="No command run yet" description={emptyText ?? "Run a command to see the parsed reply here."} />
      ) : (
        <div className="space-y-3 p-4">
          <div className="flex flex-wrap items-center gap-x-3 gap-y-1.5">
            <CommandLine args={response.command} className="min-w-0 flex-1 truncate font-mono text-[13px]" />
            <div className="flex items-center gap-2">
              {response.outcome !== "failure" && response.replies.length === 1 && <TypeTag type={response.replies[0].type} />}
              <OutcomeBadge response={response} expectError={expectError} />
              {response.outcome !== "failure" && (
                <span className="font-mono text-xs text-muted tabular-nums">{formatLatency(response.latencyMs)}</span>
              )}
            </div>
          </div>

          {response.outcome === "failure" ? (
            <p className="rounded-md border border-err/30 bg-err-soft px-3 py-2 font-mono text-xs break-words text-err">{response.error}</p>
          ) : response.outcome === "redis-error" ? (
            <p
              className={cn(
                "rounded-md border px-3 py-2 font-mono text-[13px] break-words",
                expectError ? "border-warn/30 bg-warn-soft text-warn" : "border-err/30 bg-err-soft text-err",
              )}
            >
              (error) {response.redisError}
            </p>
          ) : (
            (visualize?.(response) ?? (
              <div className="max-h-96 overflow-auto rounded-md border border-border bg-bg px-3 py-2.5">
                {response.replies.map((reply, index) => (
                  <RespValueView key={index} value={reply} />
                ))}
              </div>
            ))
          )}

          {showResp && response.outcome !== "failure" && (
            <div className="grid gap-3 md:grid-cols-2">
              <div className="min-w-0 space-y-1">
                <p className="text-[11px] font-medium text-faint uppercase">Sent</p>
                <RespRaw raw={response.rawRequest} />
              </div>
              <div className="min-w-0 space-y-1">
                <p className="text-[11px] font-medium text-faint uppercase">Received</p>
                <RespRaw raw={response.rawResponse} />
              </div>
            </div>
          )}
        </div>
      )}
    </Card>
  );
}
