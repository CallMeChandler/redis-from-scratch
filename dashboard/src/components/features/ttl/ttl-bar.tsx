import { cn } from "@/lib/cn";

const CELLS = 24;

/** Block-character countdown, e.g. ██████████████░░░░░░ */
export function TtlBar({ remaining, total }: { remaining: number; total: number }) {
  const ratio = total > 0 ? Math.max(0, Math.min(1, remaining / total)) : 0;
  const filled = Math.round(ratio * CELLS);
  const tone = ratio > 0.5 ? "text-ok" : ratio > 0.2 ? "text-warn" : "text-err";

  return (
    <span className="font-mono text-sm leading-none tracking-[-0.05em]" aria-label={`${remaining} of ${total} seconds left`}>
      <span className={cn(tone)}>{"█".repeat(filled)}</span>
      <span className="text-border-strong">{"░".repeat(CELLS - filled)}</span>
    </span>
  );
}
