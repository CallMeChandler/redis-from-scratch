"use client";

import { FlaskConical, Play, RotateCcw } from "lucide-react";
import { useState } from "react";

import { useActivity } from "@/components/providers/activity-provider";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Card, CardBody, CardHeader } from "@/components/ui/card";
import { Field, Input } from "@/components/ui/input";
import { useCommandRunner } from "@/hooks/use-command";
import { cn } from "@/lib/cn";
import { formatBytes } from "@/lib/resp/format";
import type { CommandResponse } from "@/lib/resp/types";

const KEYS = { A: "lru:A", B: "lru:B", C: "lru:C" } as const;
type Slot = keyof typeof KEYS;

type StepId = "reset" | "createA" | "createB" | "touchA" | "createC" | "check";

interface Step {
  id: StepId;
  label: string;
  detail: string;
}

const STEPS: Step[] = [
  { id: "reset", label: "Reset", detail: "DEL lru:A lru:B lru:C" },
  { id: "createA", label: "Create A", detail: "SET lru:A <size bytes>" },
  { id: "createB", label: "Create B", detail: "SET lru:B <size bytes>" },
  { id: "touchA", label: "Touch A", detail: "GET lru:A → A is now most recent" },
  { id: "createC", label: "Create C", detail: "SET lru:C → may exceed max-memory" },
  { id: "check", label: "Check", detail: "EXISTS + MEMORY USAGE for A, B, C" },
];

interface KeyState {
  exists: boolean | null;
  bytes: number | null;
  /** Logical access order on this page (1 = oldest). */
  lastAccess: number | null;
  everCreated: boolean;
}

const EMPTY: Record<Slot, KeyState> = {
  A: { exists: null, bytes: null, lastAccess: null, everCreated: false },
  B: { exists: null, bytes: null, lastAccess: null, everCreated: false },
  C: { exists: null, bytes: null, lastAccess: null, everCreated: false },
};

/** ~125 bytes of per-key overhead in the server's accounting, measured via MEMORY USAGE. */
const KEY_OVERHEAD = 125;

export function suggestValueSize(maxMemory: number): number {
  // Two keys must fit and the third must overflow: 2·(s+o) ≤ max < 3·(s+o).
  return maxMemory > 0 ? Math.max(1, Math.floor(maxMemory * 0.42) - KEY_OVERHEAD) : 1000;
}

function integer(response: CommandResponse): number | null {
  return response.outcome === "reply" && response.replies[0].type === "integer" ? response.replies[0].value : null;
}

export function LruExperiment({ maxMemory, evicted, onChanged }: { maxMemory: number; evicted: number | null; onChanged: () => void }) {
  const { run } = useCommandRunner("Memory / LRU");
  const { inspect } = useActivity();
  const [size, setSize] = useState(() => String(suggestValueSize(maxMemory)));
  const [keys, setKeys] = useState(EMPTY);
  const [done, setDone] = useState<StepId[]>([]);
  const [clock, setClock] = useState(0);
  const [running, setRunning] = useState<StepId | null>(null);
  const [evictedAtStart, setEvictedAtStart] = useState<number | null>(null);
  const [log, setLog] = useState<CommandResponse[]>([]);

  const bytes = Math.max(1, Math.min(4_000_000, Number(size) || 1));

  const exec = async (args: string[]) => {
    const response = await run(args);
    setLog((items) => [response, ...items].slice(0, 20));
    return response;
  };

  const touch = (slot: Slot, created: boolean, tick: number) =>
    setKeys((current) => ({
      ...current,
      [slot]: { ...current[slot], exists: true, lastAccess: tick, everCreated: current[slot].everCreated || created },
    }));

  const runStep = async (id: StepId, tick: number): Promise<number> => {
    setRunning(id);
    let next = tick;

    switch (id) {
      case "reset":
        await exec(["DEL", KEYS.A, KEYS.B, KEYS.C]);
        setKeys(EMPTY);
        setDone([]);
        setEvictedAtStart(evicted);
        next = 0;
        break;
      case "createA":
      case "createB":
      case "createC": {
        const slot = id.slice(-1) as Slot;
        const response = await exec(["SET", KEYS[slot], slot.toLowerCase().repeat(bytes)]);
        if (response.outcome === "reply") touch(slot, true, (next = tick + 1));
        break;
      }
      case "touchA": {
        const response = await exec(["GET", KEYS.A]);
        if (response.outcome === "reply" && response.replies[0].type === "bulk-string") touch("A", false, (next = tick + 1));
        break;
      }
      case "check": {
        const slots: Slot[] = ["A", "B", "C"];
        const exists = await Promise.all(slots.map((slot) => exec(["EXISTS", KEYS[slot]])));
        const usage = await Promise.all(slots.map((slot) => exec(["MEMORY", "USAGE", KEYS[slot]])));
        setKeys((current) => {
          const updated = { ...current };
          slots.forEach((slot, index) => {
            updated[slot] = { ...current[slot], exists: integer(exists[index]) === 1, bytes: integer(usage[index]) };
          });
          return updated;
        });
        onChanged();
        break;
      }
    }

    setDone((items) => (id === "reset" ? ["reset"] : [...new Set([...items, id])]));
    setClock(next);
    setRunning(null);
    return next;
  };

  const runAll = async () => {
    let tick = 0;
    for (const step of STEPS) tick = await runStep(step.id, tick);
  };

  const order = (["A", "B", "C"] as Slot[])
    .filter((slot) => keys[slot].lastAccess !== null)
    .sort((a, b) => (keys[a].lastAccess ?? 0) - (keys[b].lastAccess ?? 0));

  return (
    <Card>
      <CardHeader
        icon={<FlaskConical className="size-4" />}
        title="LRU eviction experiment"
        description="Fill memory with A and B, touch A, then add C. With all-keys LRU the least recently used key (B) should be evicted."
        actions={
          <Button variant="primary" icon={<Play className="size-3.5" />} onClick={runAll} disabled={running !== null}>
            Run all steps
          </Button>
        }
      />
      <CardBody className="space-y-5">
        <div className="flex flex-wrap items-end gap-3">
          <Field label="Value size (bytes)" hint={maxMemory > 0 ? `suggested ${suggestValueSize(maxMemory)}` : undefined} className="w-60">
            <Input value={size} inputMode="numeric" onChange={(event) => setSize(event.target.value.replace(/\D/g, ""))} />
          </Field>
          <Button variant="ghost" icon={<RotateCcw className="size-3.5" />} onClick={() => setSize(String(suggestValueSize(maxMemory)))}>
            Use suggestion
          </Button>
          <p className="max-w-md text-xs text-faint">
            Each key costs about value + {KEY_OVERHEAD} bytes of logical memory. Other keys in the dataset also count toward max-memory and, being older, may be evicted first.
          </p>
        </div>

        <ol className="grid gap-2 sm:grid-cols-2 lg:grid-cols-3">
          {STEPS.map((step, index) => (
            <li key={step.id}>
              <button
                type="button"
                disabled={running !== null}
                onClick={() => runStep(step.id, clock)}
                className={cn(
                  "flex w-full items-start gap-2.5 rounded-lg border px-3 py-2 text-left transition-colors disabled:opacity-60",
                  done.includes(step.id) ? "border-ok/30 bg-ok-soft" : "border-border bg-panel-2 hover:border-border-strong",
                  running === step.id && "border-accent",
                )}
              >
                <span className="mt-0.5 font-mono text-[11px] text-faint">{index}</span>
                <span className="min-w-0">
                  <span className="block text-sm font-medium text-text">{step.label}</span>
                  <span className="block truncate font-mono text-[11px] text-muted">{step.detail}</span>
                </span>
              </button>
            </li>
          ))}
        </ol>

        <div className="grid gap-3 sm:grid-cols-3">
          {(["A", "B", "C"] as Slot[]).map((slot) => {
            const state = keys[slot];
            const evictedNow = state.everCreated && state.exists === false;
            return (
              <div
                key={slot}
                className={cn(
                  "rounded-lg border px-4 py-3",
                  evictedNow ? "border-err/40 bg-err-soft" : state.exists ? "border-ok/30 bg-panel-2" : "border-border bg-panel-2",
                )}
              >
                <div className="flex items-center justify-between">
                  <code className="font-mono text-sm font-semibold text-text">{KEYS[slot]}</code>
                  {evictedNow ? (
                    <Badge tone="err">evicted</Badge>
                  ) : state.exists ? (
                    <Badge tone="ok">exists</Badge>
                  ) : (
                    <Badge>{state.exists === false ? "absent" : "unknown"}</Badge>
                  )}
                </div>
                <dl className="mt-2 space-y-0.5 font-mono text-[11px] text-muted">
                  <div className="flex justify-between">
                    <dt>last access</dt>
                    <dd>{state.lastAccess === null ? "—" : `t${state.lastAccess}`}</dd>
                  </div>
                  <div className="flex justify-between">
                    <dt>MEMORY USAGE</dt>
                    <dd>{state.bytes === null ? "—" : formatBytes(state.bytes)}</dd>
                  </div>
                </dl>
              </div>
            );
          })}
        </div>

        <div className="flex flex-wrap items-center gap-x-6 gap-y-2 rounded-lg border border-border bg-bg px-4 py-2.5 font-mono text-xs">
          <span className="text-faint">recency (oldest → newest)</span>
          <span className="text-text">{order.length ? order.map((slot) => `${slot}@t${keys[slot].lastAccess}`).join("  →  ") : "—"}</span>
          <span className="ml-auto text-muted">
            evicted_keys {evicted ?? "—"}
            {evictedAtStart !== null && evicted !== null && evicted > evictedAtStart && <span className="ml-1 text-warn">(+{evicted - evictedAtStart} this run)</span>}
          </span>
        </div>

        {log.length > 0 && (
          <div className="flex flex-wrap gap-1">
            {log.slice(0, 12).map((item) => (
              <button
                key={item.id}
                type="button"
                onClick={() => inspect(item)}
                className={cn(
                  "rounded border px-1.5 py-0.5 font-mono text-[10.5px]",
                  item.outcome === "reply" ? "border-border text-muted hover:text-text" : "border-err/30 text-err",
                )}
              >
                {item.command.slice(0, 2).join(" ")}
              </button>
            ))}
          </div>
        )}
      </CardBody>
    </Card>
  );
}
