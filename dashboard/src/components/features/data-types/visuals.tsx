import type { ReactNode } from "react";

import { Card } from "@/components/ui/card";
import { cn } from "@/lib/cn";
import type { CommandResponse, RespValue } from "@/lib/resp/types";

export function asStrings(value: RespValue | undefined): string[] | null {
  if (value?.type !== "array") return null;
  return value.value.map((item) => (item.type === "bulk-string" || item.type === "simple-string" ? item.value : "(nil)"));
}

export function replyOf(response: CommandResponse | undefined): RespValue | undefined {
  return response && response.outcome !== "failure" ? response.replies[0] : undefined;
}

export function errorOf(response: CommandResponse | undefined): string | null {
  if (!response) return null;
  if (response.outcome === "failure") return response.error;
  if (response.outcome === "redis-error") return response.redisError;
  return null;
}

/** Frame for a "live view of key X" panel. */
export function SnapshotCard({
  title,
  keyName,
  meta,
  children,
}: {
  title: string;
  keyName: string;
  meta?: ReactNode;
  children: ReactNode;
}) {
  return (
    <Card>
      <header className="flex items-center justify-between gap-3 border-b border-border px-4 py-2.5">
        <div className="flex min-w-0 items-center gap-2">
          <span className="size-1.5 rounded-full bg-ok" />
          <h2 className="text-[11px] font-medium tracking-wide text-faint uppercase">{title}</h2>
          <code className="truncate font-mono text-xs text-text">{keyName || "—"}</code>
        </div>
        {meta && <div className="shrink-0 font-mono text-[11px] text-muted">{meta}</div>}
      </header>
      <div className="p-4">{children}</div>
    </Card>
  );
}

export function SnapshotMessage({ children, tone = "muted" }: { children: ReactNode; tone?: "muted" | "err" }) {
  return <p className={cn("font-mono text-xs", tone === "err" ? "text-err" : "text-faint")}>{children}</p>;
}

/** Horizontal list cells with positive and negative indices (LRANGE). */
export function ListCells({ items }: { items: string[] }) {
  if (items.length === 0) return <SnapshotMessage>(empty list)</SnapshotMessage>;

  return (
    <div className="flex flex-wrap items-stretch gap-1.5">
      {items.map((item, index) => (
        <div key={index} className="flex min-w-14 flex-col overflow-hidden rounded-md border border-border-strong bg-panel-2">
          <div className="flex justify-between gap-2 border-b border-border px-1.5 py-0.5 font-mono text-[10px] text-faint">
            <span>{index}</span>
            <span>{index - items.length}</span>
          </div>
          <div className="max-w-48 truncate px-2 py-1.5 font-mono text-xs text-t-string" title={item}>
            {item}
          </div>
        </div>
      ))}
    </div>
  );
}

/** Set members as chips. */
export function MemberChips({ items, highlight }: { items: string[]; highlight?: string }) {
  if (items.length === 0) return <SnapshotMessage>(empty set)</SnapshotMessage>;

  return (
    <div className="flex flex-wrap gap-1.5">
      {[...items].sort().map((item) => (
        <span
          key={item}
          className={cn(
            "max-w-56 truncate rounded-full border px-2.5 py-0.5 font-mono text-xs",
            item === highlight ? "border-accent bg-accent-soft text-accent" : "border-border-strong bg-panel-2 text-t-string",
          )}
          title={item}
        >
          {item}
        </span>
      ))}
    </div>
  );
}

/** Sorted-set members in rank order, with optional scores. */
export function RankedMembers({ members, scores }: { members: string[]; scores?: Record<string, string> }) {
  if (members.length === 0) return <SnapshotMessage>(empty sorted set)</SnapshotMessage>;

  const numeric = scores ? members.map((member) => Number(scores[member])).filter(Number.isFinite) : [];
  const max = numeric.length ? Math.max(...numeric.map(Math.abs), 1) : 1;

  return (
    <ol className="divide-y divide-border overflow-hidden rounded-md border border-border">
      {members.map((member, rank) => {
        const score = scores?.[member];
        const width = score !== undefined && Number.isFinite(Number(score)) ? (Math.abs(Number(score)) / max) * 100 : 0;
        return (
          <li key={member} className="relative flex items-center gap-3 px-3 py-1.5 font-mono text-xs">
            <span className="absolute inset-y-0 left-0 bg-accent-soft" style={{ width: `${width}%` }} aria-hidden />
            <span className="relative w-6 text-right text-faint tabular-nums">#{rank}</span>
            <span className="relative min-w-0 flex-1 truncate text-t-string">{member}</span>
            {score !== undefined && <span className="relative text-t-integer tabular-nums">{score}</span>}
          </li>
        );
      })}
    </ol>
  );
}
