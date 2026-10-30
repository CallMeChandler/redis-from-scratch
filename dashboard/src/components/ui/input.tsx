import type { CSSProperties, InputHTMLAttributes, ReactNode, SelectHTMLAttributes } from "react";

import { cn } from "@/lib/cn";

const FIELD =
  "h-8.5 w-full rounded-md border border-border-strong bg-panel-2 px-2.5 font-mono text-sm text-text placeholder:text-faint transition-colors hover:border-faint focus:border-accent focus:outline-none disabled:opacity-50";

export function Input({ className, ...props }: InputHTMLAttributes<HTMLInputElement>) {
  return <input className={cn(FIELD, className)} spellCheck={false} autoComplete="off" {...props} />;
}

export function Select({ className, children, ...props }: SelectHTMLAttributes<HTMLSelectElement>) {
  return (
    <select className={cn(FIELD, "pr-7 font-sans", className)} {...props}>
      {children}
    </select>
  );
}

export function Field({
  label,
  hint,
  children,
  className,
  style,
}: {
  label: string;
  hint?: ReactNode;
  children: ReactNode;
  className?: string;
  style?: CSSProperties;
}) {
  return (
    <label className={cn("flex min-w-0 flex-col gap-1", className)} style={style}>
      <span className="flex items-baseline justify-between gap-2 text-[11px] font-medium tracking-wide text-muted uppercase">
        {label}
        {hint && <span className="font-normal tracking-normal normal-case text-faint">{hint}</span>}
      </span>
      {children}
    </label>
  );
}
