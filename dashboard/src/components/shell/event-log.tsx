"use client";

import { ChevronDown, ChevronUp, ScrollText, Trash2 } from "lucide-react";
import { useState } from "react";

import { responseSummary } from "@/components/command/history-list";
import { useActivity } from "@/components/providers/activity-provider";
import { cn } from "@/lib/cn";
import { displayCommand, formatLatency } from "@/lib/resp/format";

/** Bottom activity panel: every command the dashboard sent, newest first. */
export function EventLog() {
  const { entries, clear, inspect } = useActivity();
  const [open, setOpen] = useState(false);
  const [showAuto, setShowAuto] = useState(false);

  const visible = showAuto ? entries : entries.filter((entry) => !entry.auto);
  const hidden = entries.length - visible.length;
  const latest = visible[0];

  return (
    <section className="shrink-0 border-t border-border bg-panel" aria-label="Event log">
      <div className="flex h-9 items-center gap-3 px-3 md:px-4">
        <button
          type="button"
          onClick={() => setOpen(!open)}
          className="flex items-center gap-2 text-xs font-medium text-muted hover:text-text"
          aria-expanded={open}
        >
          <ScrollText className="size-3.5" />
          Event log
          <span className="rounded bg-elevated px-1.5 font-mono text-[10px] text-faint">{visible.length}</span>
          {open ? <ChevronDown className="size-3.5" /> : <ChevronUp className="size-3.5" />}
        </button>

        {!open && latest && (
          <button
            type="button"
            onClick={() => inspect(latest.response)}
            className="hidden min-w-0 flex-1 truncate text-left font-mono text-[11px] text-faint hover:text-muted md:block"
          >
            {new Date(latest.response.startedAt).toLocaleTimeString([], { hour12: false })} ·{" "}
            {latest.response.target.toUpperCase()} · {displayCommand(latest.response.command)} →{" "}
            {responseSummary(latest.response)}
          </button>
        )}

        <div className="ml-auto flex items-center gap-2">
          <label className="flex items-center gap-1.5 text-[11px] text-faint">
            <input type="checkbox" checked={showAuto} onChange={(event) => setShowAuto(event.target.checked)} className="accent-[var(--accent)]" />
            show auto refresh{hidden > 0 && !showAuto ? ` (${hidden})` : ""}
          </label>
          <button
            type="button"
            onClick={clear}
            className="rounded p-1 text-faint hover:bg-hover hover:text-text"
            aria-label="Clear event log"
            title="Clear"
          >
            <Trash2 className="size-3.5" />
          </button>
        </div>
      </div>

      {open && (
        <div className="h-56 overflow-auto border-t border-border">
          {visible.length === 0 ? (
            <p className="p-4 text-center text-xs text-faint">No commands yet. Everything the dashboard sends shows up here.</p>
          ) : (
            <table className="w-full table-fixed font-mono text-[11.5px]">
              <tbody>
                {visible.map(({ response, origin, auto }) => (
                  <tr
                    key={response.id}
                    onClick={() => inspect(response)}
                    className="cursor-pointer border-b border-border/60 hover:bg-hover"
                  >
                    <td className="w-20 py-1 pl-4 text-faint tabular-nums">
                      {new Date(response.startedAt).toLocaleTimeString([], { hour12: false })}
                    </td>
                    <td className="w-20 py-1">
                      <span className={response.target === "primary" ? "text-accent" : "text-t-array"}>{response.target.toUpperCase()}</span>
                    </td>
                    <td className="hidden w-28 truncate py-1 text-faint lg:table-cell">
                      {response.via === "session" ? "session" : "http"} · {origin}
                      {auto ? " · auto" : ""}
                    </td>
                    <td className="truncate py-1 pr-3 text-text">{displayCommand(response.command)}</td>
                    <td
                      className={cn(
                        "w-[30%] truncate py-1 pr-3",
                        response.outcome === "reply" ? "text-muted" : response.outcome === "redis-error" ? "text-warn" : "text-err",
                      )}
                    >
                      {responseSummary(response)}
                    </td>
                    <td className="w-20 py-1 pr-4 text-right text-faint tabular-nums">
                      {response.outcome === "failure" ? "—" : formatLatency(response.latencyMs)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      )}
    </section>
  );
}
