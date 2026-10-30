import type { ReactNode } from "react";

import { cn } from "@/lib/cn";

import type { Tone } from "./badge";

const VALUE_TONE: Record<Tone, string> = {
  neutral: "text-text",
  ok: "text-ok",
  err: "text-err",
  warn: "text-warn",
  accent: "text-accent",
};

export function StatCard({
  label,
  value,
  sub,
  icon,
  tone = "neutral",
  className,
}: {
  label: string;
  value: ReactNode;
  sub?: ReactNode;
  icon?: ReactNode;
  tone?: Tone;
  className?: string;
}) {
  return (
    <div className={cn("rounded-xl border border-border bg-panel px-4 py-3.5", className)}>
      <div className="flex items-center justify-between gap-2">
        <p className="text-[11px] font-medium tracking-wide text-muted uppercase">{label}</p>
        {icon && <span className="text-faint">{icon}</span>}
      </div>
      <p className={cn("mt-1.5 truncate font-mono text-[22px] leading-tight font-semibold tabular-nums", VALUE_TONE[tone])}>{value}</p>
      {sub && <p className="mt-1 truncate text-xs text-faint">{sub}</p>}
    </div>
  );
}
