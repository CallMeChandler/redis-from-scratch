import type { ReactNode } from "react";

import { cn } from "@/lib/cn";

export type Tone = "neutral" | "ok" | "err" | "warn" | "accent";

const TONES: Record<Tone, string> = {
  neutral: "bg-elevated text-muted border-border-strong",
  ok: "bg-ok-soft text-ok border-ok/25",
  err: "bg-err-soft text-err border-err/25",
  warn: "bg-warn-soft text-warn border-warn/25",
  accent: "bg-accent-soft text-accent border-accent/25",
};

export function Badge({ tone = "neutral", className, children }: { tone?: Tone; className?: string; children: ReactNode }) {
  return (
    <span
      className={cn(
        "inline-flex items-center gap-1 rounded border px-1.5 py-px font-mono text-[10.5px] font-medium tracking-wide whitespace-nowrap uppercase",
        TONES[tone],
        className,
      )}
    >
      {children}
    </span>
  );
}

const DOTS: Record<Tone, string> = {
  neutral: "bg-faint",
  ok: "bg-ok",
  err: "bg-err",
  warn: "bg-warn",
  accent: "bg-accent",
};

export function StatusDot({ tone, pulse = false }: { tone: Tone; pulse?: boolean }) {
  return <span className={cn("inline-block size-2 shrink-0 rounded-full", DOTS[tone], pulse && "animate-pulse-dot")} />;
}
