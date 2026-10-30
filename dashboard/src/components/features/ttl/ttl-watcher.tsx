"use client";

import { Pause, Play, X } from "lucide-react";
import { useEffect, useState } from "react";

import type { SnapshotContext } from "@/components/command/command-playground";
import { Card } from "@/components/ui/card";
import { useCommandRunner } from "@/hooks/use-command";
import { cn } from "@/lib/cn";

import { TtlBar } from "./ttl-bar";

interface Sample {
  ttl: number | null;
  /** Largest TTL seen since the key last got an expiry: the bar's 100%. */
  total: number;
}

const TTL_COMMANDS = new Set(["EXPIRE", "EXPIREAT", "TTL", "SET"]);
const MAX_KEYS = 8;

/** Polls TTL once per second for every key touched on the page. */
export function TtlWatcher({ shared, target, history }: SnapshotContext) {
  const { run } = useCommandRunner("TTL");
  const [paused, setPaused] = useState(false);
  const [removed, setRemoved] = useState<string[]>([]);
  const [samples, setSamples] = useState<Record<string, Sample>>({});
  // Starts empty so server-prerendered HTML matches the first client render.
  const [now, setNow] = useState<number | null>(null);

  const keys = [
    ...new Set([
      ...history.filter((item) => TTL_COMMANDS.has(item.command[0]?.toUpperCase())).map((item) => item.command[1]),
      shared.key ?? "",
    ]),
  ]
    .filter((key) => key && !removed.includes(key))
    .slice(0, MAX_KEYS);

  const signature = JSON.stringify(keys);
  const lastExpire = history.find((item) => ["EXPIRE", "EXPIREAT"].includes(item.command[0]?.toUpperCase()))?.id;

  useEffect(() => {
    if (paused) return;
    let cancelled = false;
    // The first sample after (re)starting, e.g. right after a new EXPIRE,
    // resets each bar's 100% mark.
    let fresh = true;
    const watched = JSON.parse(signature) as string[];

    const poll = async () => {
      setNow(Math.floor(Date.now() / 1000));
      const results = await Promise.all(watched.map((key) => run(["TTL", key], target, { auto: true })));
      if (cancelled) return;
      const reset = fresh;
      fresh = false;

      setSamples((current) => {
        const next: Record<string, Sample> = {};
        watched.forEach((key, index) => {
          const response = results[index];
          const reply = response.outcome === "reply" ? response.replies[0] : undefined;
          const ttl = reply?.type === "integer" ? reply.value : null;
          const previous = reset ? undefined : current[key];
          const carried = previous?.ttl && previous.ttl > 0 ? previous.total : 0;
          next[key] = { ttl, total: ttl !== null && ttl > 0 ? Math.max(ttl, carried) : 0 };
        });
        return next;
      });
    };

    void poll();
    const timer = setInterval(poll, 1000);
    return () => {
      cancelled = true;
      clearInterval(timer);
    };
  }, [signature, target, paused, run, lastExpire]);

  return (
    <Card>
      <header className="flex items-center justify-between gap-3 border-b border-border px-4 py-2.5">
        <div className="flex items-center gap-2">
          <span className={cn("size-1.5 rounded-full", paused ? "bg-faint" : "animate-pulse-dot bg-ok")} />
          <h2 className="text-[11px] font-medium tracking-wide text-faint uppercase">TTL watcher · 1 s polling</h2>
        </div>
        <div className="flex items-center gap-2">
          <span className="font-mono text-[11px] text-faint" title="Current unix time (seconds), useful for EXPIREAT">
            now {now ?? "—"}
          </span>
          <button
            type="button"
            onClick={() => setPaused(!paused)}
            className="rounded p-1 text-muted hover:bg-hover hover:text-text"
            aria-label={paused ? "Resume polling" : "Pause polling"}
          >
            {paused ? <Play className="size-3.5" /> : <Pause className="size-3.5" />}
          </button>
        </div>
      </header>

      {keys.length === 0 ? (
        <p className="p-4 font-mono text-xs text-faint">Type a key or run a command to watch its TTL.</p>
      ) : (
        <ul className="divide-y divide-border">
          {keys.map((key) => {
            const sample = samples[key];
            const ttl = sample?.ttl ?? null;
            return (
              <li key={key} className="group flex flex-col gap-1.5 px-4 py-3">
                <div className="flex items-center justify-between gap-3">
                  <code className="truncate font-mono text-[13px] text-text">{key}</code>
                  <div className="flex items-center gap-2">
                    <span
                      className={cn(
                        "font-mono text-xs",
                        ttl === null ? "text-faint" : ttl === -2 ? "text-err" : ttl === -1 ? "text-muted" : "text-text",
                      )}
                    >
                      {ttl === null ? "…" : ttl === -2 ? "TTL -2 · missing / expired" : ttl === -1 ? "TTL -1 · no expiry" : `TTL: ${ttl} seconds`}
                    </span>
                    <button
                      type="button"
                      onClick={() => setRemoved([...removed, key])}
                      className="rounded p-0.5 text-faint opacity-0 group-hover:opacity-100 hover:text-text"
                      aria-label={`Stop watching ${key}`}
                    >
                      <X className="size-3.5" />
                    </button>
                  </div>
                </div>
                {ttl !== null && ttl > 0 ? (
                  <div className="flex items-center gap-3">
                    <TtlBar remaining={ttl} total={sample.total} />
                    {now !== null && (
                      <span className="font-mono text-[11px] text-faint">
                        expires ≈ {new Date((now + ttl) * 1000).toLocaleTimeString([], { hour12: false })}
                      </span>
                    )}
                  </div>
                ) : (
                  <TtlBar remaining={0} total={1} />
                )}
              </li>
            );
          })}
        </ul>
      )}
    </Card>
  );
}
