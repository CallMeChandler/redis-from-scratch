"use client";

import { ChevronRight } from "lucide-react";
import { useState, type ReactNode } from "react";

import { cn } from "@/lib/cn";

export function Collapsible({
  title,
  children,
  defaultOpen = false,
  actions,
  className,
}: {
  title: ReactNode;
  children: ReactNode;
  defaultOpen?: boolean;
  actions?: ReactNode;
  className?: string;
}) {
  const [open, setOpen] = useState(defaultOpen);

  return (
    <div className={cn("rounded-lg border border-border bg-panel", className)}>
      <div className="flex items-center justify-between gap-2 pr-2">
        <button
          type="button"
          onClick={() => setOpen(!open)}
          aria-expanded={open}
          className="flex flex-1 items-center gap-1.5 px-3 py-2 text-left text-xs font-medium text-muted hover:text-text"
        >
          <ChevronRight className={cn("size-3.5 transition-transform", open && "rotate-90")} />
          {title}
        </button>
        {actions}
      </div>
      {open && <div className="border-t border-border">{children}</div>}
    </div>
  );
}
