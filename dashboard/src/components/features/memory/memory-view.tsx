"use client";

import { Database, Gauge, KeyRound, TriangleAlert, Trash2 } from "lucide-react";
import { useState, type FormEvent } from "react";

import { ResultCard } from "@/components/command/result-card";
import { Button } from "@/components/ui/button";
import { Card, CardBody, CardHeader } from "@/components/ui/card";
import { Field, Input } from "@/components/ui/input";
import { PageHeader } from "@/components/ui/page-header";
import { ServerOffline } from "@/components/ui/server-offline";
import { StatCard } from "@/components/ui/stat-card";
import { useCommandRunner } from "@/hooks/use-command";
import { useInfo } from "@/hooks/use-info";
import { numberField, parseKeyspace } from "@/lib/info";
import { formatBytes } from "@/lib/resp/format";
import type { CommandResponse, CommandRoundTrip } from "@/lib/resp/types";

import { LruExperiment } from "./lru-experiment";

function usageVisual(response: CommandRoundTrip) {
  const reply = response.replies[0];
  if (reply.type !== "integer") return undefined;
  return (
    <div className="flex items-baseline gap-3 rounded-md border border-border bg-bg px-3 py-3">
      <span className="font-mono text-2xl font-semibold text-t-integer">{reply.value.toLocaleString()}</span>
      <span className="text-sm text-muted">bytes ({formatBytes(reply.value)}) accounted to {response.command[2]}</span>
    </div>
  );
}

export function MemoryView() {
  const { data, fields, reload } = useInfo("primary", undefined, 3000, "Memory / LRU");
  const { run, pending } = useCommandRunner("Memory / LRU");
  const [key, setKey] = useState("lru:A");
  const [usage, setUsage] = useState<CommandResponse | null>(null);

  const used = numberField(fields, "used_memory") ?? 0;
  const max = numberField(fields, "maxmemory") ?? 0;
  const keyspace = parseKeyspace(fields.db0);
  const ratio = max > 0 ? Math.min(1, used / max) : 0;

  const memoryUsage = async (event: FormEvent) => {
    event.preventDefault();
    setUsage(await run(["MEMORY", "USAGE", key]));
  };

  return (
    <>
      <PageHeader
        title="Memory / LRU"
        description="The server tracks an approximate logical footprint per key and, above max-memory, evicts the least recently used key using an exact access clock."
        commands={["INFO memory", "INFO stats", "MEMORY USAGE"]}
      />

      {data && !data.ok ? (
        <ServerOffline target="primary" error={data.error} />
      ) : (
        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-3 lg:grid-cols-4">
            <StatCard label="Used memory" icon={<Gauge className="size-4" />} value={formatBytes(used)} sub={`${used.toLocaleString()} bytes`} />
            <StatCard label="Max memory" icon={<Database className="size-4" />} value={max > 0 ? formatBytes(max) : "unlimited"} tone={max > 0 ? "neutral" : "warn"} sub={max > 0 ? `${max.toLocaleString()} bytes` : "max-memory 0"} />
            <StatCard label="Evicted keys" icon={<Trash2 className="size-4" />} value={numberField(fields, "evicted_keys") ?? "—"} tone={(numberField(fields, "evicted_keys") ?? 0) > 0 ? "warn" : "neutral"} />
            <StatCard label="Keys" icon={<KeyRound className="size-4" />} value={keyspace?.keys ?? "—"} sub={keyspace ? `${keyspace.expires} with TTL` : undefined} />
          </div>

          {max > 0 ? (
            <div className="rounded-xl border border-border bg-panel px-4 py-3">
              <div className="mb-2 flex justify-between font-mono text-xs text-muted">
                <span>used_memory / maxmemory</span>
                <span>{(ratio * 100).toFixed(1)}%</span>
              </div>
              <div className="h-2 overflow-hidden rounded-full bg-elevated">
                <div className={`h-full rounded-full transition-all ${ratio > 0.9 ? "bg-err" : ratio > 0.7 ? "bg-warn" : "bg-ok"}`} style={{ width: `${ratio * 100}%` }} />
              </div>
            </div>
          ) : (
            data && (
              <div className="flex gap-3 rounded-xl border border-warn/30 bg-warn-soft px-4 py-3 text-sm">
                <TriangleAlert className="mt-0.5 size-4 shrink-0 text-warn" />
                <div>
                  <p className="font-medium text-warn">Eviction is disabled (max-memory 0)</p>
                  <p className="mt-1 text-xs text-muted">
                    The server has no CONFIG SET, so the limit comes from its config file. Restart the primary with a limit, e.g.{" "}
                    <code className="font-mono text-text">./build/redis-server dashboard/dev/lru.conf</code> (max-memory 3000). The experiment still runs, but nothing is evicted.
                  </p>
                </div>
              </div>
            )
          )}

          {data?.ok && <LruExperiment key={max} maxMemory={max} evicted={numberField(fields, "evicted_keys")} onChanged={reload} />}

          <div className="grid gap-4 lg:grid-cols-2">
            <Card>
              <CardHeader title="MEMORY USAGE" description="Logical bytes for a single key: key + value + per-element overhead" />
              <CardBody>
                <form onSubmit={memoryUsage} className="flex items-end gap-2">
                  <Field label="Key" className="flex-1">
                    <Input value={key} onChange={(event) => setKey(event.target.value)} />
                  </Field>
                  <Button type="submit" loading={pending} disabled={!key}>
                    Run
                  </Button>
                </form>
              </CardBody>
            </Card>
            <ResultCard response={usage} visualize={usageVisual} emptyText="Returns (nil) for keys that do not exist." />
          </div>
        </div>
      )}
    </>
  );
}
