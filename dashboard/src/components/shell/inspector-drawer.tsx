"use client";

import { X } from "lucide-react";
import { useEffect } from "react";

import { CommandInspector } from "@/components/command/command-inspector";
import { useActivity } from "@/components/providers/activity-provider";

/** Global command inspector, opened from any result, history row or log entry. */
export function InspectorDrawer() {
  const { inspected, inspect } = useActivity();

  useEffect(() => {
    if (!inspected) return;
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") inspect(null);
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [inspected, inspect]);

  if (!inspected) return null;

  return (
    <>
      <div className="fixed inset-0 z-50 bg-black/40" onClick={() => inspect(null)} aria-hidden />
      <aside
        role="dialog"
        aria-label="Command inspector"
        className="animate-slide-in fixed top-0 right-0 bottom-0 z-50 flex w-full max-w-lg flex-col border-l border-border-strong bg-panel shadow-pop"
      >
        <header className="flex h-12 shrink-0 items-center justify-between border-b border-border px-4">
          <h2 className="text-sm font-semibold">Command inspector</h2>
          <button
            type="button"
            onClick={() => inspect(null)}
            className="rounded p-1 text-muted hover:bg-hover hover:text-text"
            aria-label="Close inspector"
          >
            <X className="size-4" />
          </button>
        </header>
        <div className="flex-1 overflow-y-auto p-4">
          <CommandInspector response={inspected} />
        </div>
      </aside>
    </>
  );
}
