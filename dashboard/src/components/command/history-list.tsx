"use client";

import { History } from "lucide-react";

import { Card } from "@/components/ui/card";
import { EmptyState } from "@/components/ui/empty-state";
import { cn } from "@/lib/cn";
import { formatLatency, summarize } from "@/lib/resp/format";
import type { CommandResponse } from "@/lib/resp/types";

import { CommandLine } from "./command-line";

export function responseSummary(response: CommandResponse): string {
  if (response.outcome === "failure") return response.error;
  if (response.outcome === "redis-error") return response.redisError;
  return response.replies.length === 1 ? summarize(response.replies[0]) : `${response.replies.length} frames`;
}

/** Recent commands for one page; clicking an entry re-displays its result. */
export function HistoryList({
  items,
  selectedId,
  onSelect,
}: {
  items: CommandResponse[];
  selectedId?: string;
  onSelect: (response: CommandResponse) => void;
}) {
  return (
    <Card>
      <header className="flex items-center justify-between border-b border-border px-4 py-2.5">
        <h2 className="text-[11px] font-medium tracking-wide text-faint uppercase">Recent on this page</h2>
        <span className="font-mono text-[11px] text-faint">{items.length}</span>
      </header>
      {items.length === 0 ? (
        <EmptyState icon={<History className="size-5" />} title="Nothing yet" className="py-6" />
      ) : (
        <ul className="max-h-72 divide-y divide-border overflow-auto">
          {items.map((item) => (
            <li key={item.id}>
              <button
                type="button"
                onClick={() => onSelect(item)}
                className={cn(
                  "flex w-full items-center gap-3 px-4 py-1.5 text-left transition-colors hover:bg-hover",
                  item.id === selectedId && "bg-accent-soft",
                )}
              >
                <span
                  className={cn(
                    "size-1.5 shrink-0 rounded-full",
                    item.outcome === "reply" ? "bg-ok" : item.outcome === "redis-error" ? "bg-warn" : "bg-err",
                  )}
                />
                <CommandLine args={item.command} className="min-w-0 flex-1 truncate font-mono text-xs" />
                <span className="max-w-[40%] truncate font-mono text-[11px] text-muted">{responseSummary(item)}</span>
                {item.outcome !== "failure" && (
                  <span className="w-14 shrink-0 text-right font-mono text-[11px] text-faint tabular-nums">{formatLatency(item.latencyMs)}</span>
                )}
              </button>
            </li>
          ))}
        </ul>
      )}
    </Card>
  );
}
