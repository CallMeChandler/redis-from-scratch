"use client";

import {
  ArrowDownToLine,
  ArrowUpFromLine,
  Clock3,
  Database,
  Gauge,
  KeyRound,
  Network,
  Timer,
  Trash2,
  Users,
  Zap,
} from "lucide-react";
import { useState } from "react";

import { TargetToggle } from "@/components/command/target-toggle";
import { Card, CardHeader } from "@/components/ui/card";
import { Collapsible } from "@/components/ui/collapsible";
import { CopyButton } from "@/components/ui/copy-button";
import { PageHeader } from "@/components/ui/page-header";
import { Segmented } from "@/components/ui/segmented";
import { ServerOffline } from "@/components/ui/server-offline";
import { StatCard } from "@/components/ui/stat-card";
import { useInfo } from "@/hooks/use-info";
import { numberField, parseKeyspace } from "@/lib/info";
import { formatBytes, formatDuration } from "@/lib/resp/format";
import type { Target } from "@/lib/resp/types";

const REFRESH_OPTIONS = [
  { value: "0", label: "Off" },
  { value: "2000", label: "2s" },
  { value: "5000", label: "5s" },
  { value: "10000", label: "10s" },
] as const;

type RefreshValue = (typeof REFRESH_OPTIONS)[number]["value"];

function delta(current: number | null, previous: number | null): string | undefined {
  if (current === null || previous === null) return undefined;
  const change = current - previous;
  return change === 0 ? "no change since last sample" : `${change > 0 ? "+" : ""}${change.toLocaleString()} since last sample`;
}

export function OverviewView() {
  const [target, setTarget] = useState<Target>("primary");
  const [refresh, setRefresh] = useState<RefreshValue>("5000");
  const { data, fields, previousFields, loadedAt } = useInfo(target, undefined, Number(refresh), "Overview");

  const n = (key: string) => numberField(fields, key);
  const p = (key: string) => (previousFields ? numberField(previousFields, key) : null);
  const keyspace = parseKeyspace(fields.db0) ?? { keys: 0, expires: 0 };
  const maxMemory = n("maxmemory") ?? 0;
  const usedMemory = n("used_memory") ?? 0;
  const role = fields.role ?? "—";

  return (
    <>
      <PageHeader
        title="Overview"
        description="Live runtime metrics parsed from INFO. Each refresh is a real INFO command over RESP."
        actions={
          <>
            <TargetToggle value={target} onChange={setTarget} size="md" />
            <Segmented label="Auto refresh" value={refresh} onChange={setRefresh} options={REFRESH_OPTIONS} />
          </>
        }
      />

      {data === null ? (
        <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
          {Array.from({ length: 8 }, (_, index) => (
            <div key={index} className="h-[92px] animate-pulse rounded-xl border border-border bg-panel" />
          ))}
        </div>
      ) : !data.ok ? (
        <ServerOffline target={target} error={data.error} />
      ) : (
        <div className="space-y-5">
          <div className="grid grid-cols-2 gap-3 md:grid-cols-3 xl:grid-cols-4">
            <StatCard label="Uptime" icon={<Clock3 className="size-4" />} value={formatDuration(n("uptime_in_seconds") ?? NaN)} sub={`${n("uptime_in_seconds")?.toLocaleString() ?? "—"} seconds`} />
            <StatCard label="Connected clients" icon={<Users className="size-4" />} value={n("connected_clients") ?? "—"} sub={`${n("total_connections_received")?.toLocaleString() ?? "—"} received · ${n("rejected_connections") ?? 0} rejected`} />
            <StatCard label="Commands processed" icon={<Zap className="size-4" />} value={n("total_commands_processed")?.toLocaleString() ?? "—"} sub={delta(n("total_commands_processed"), p("total_commands_processed"))} />
            <StatCard
              label="Role"
              icon={<Network className="size-4" />}
              tone={role === "primary" ? "accent" : "neutral"}
              value={role}
              sub={
                role === "replica"
                  ? `primary ${fields.primary_host}:${fields.primary_port} · link ${fields.primary_link_status}`
                  : `${n("connected_replicas") ?? 0} connected replica(s)`
              }
            />
            <StatCard label="Used memory" icon={<Gauge className="size-4" />} value={formatBytes(usedMemory)} sub={maxMemory > 0 ? `${((usedMemory / maxMemory) * 100).toFixed(1)}% of max-memory` : "logical estimate"} />
            <StatCard label="Max memory" icon={<Database className="size-4" />} value={maxMemory > 0 ? formatBytes(maxMemory) : "unlimited"} sub={maxMemory > 0 ? "all-keys LRU eviction" : "max-memory 0 · eviction off"} tone={maxMemory > 0 ? "neutral" : "warn"} />
            <StatCard label="Keys" icon={<KeyRound className="size-4" />} value={keyspace.keys.toLocaleString()} sub="db0" />
            <StatCard label="Keys with TTL" icon={<Timer className="size-4" />} value={keyspace.expires.toLocaleString()} sub={keyspace.keys > 0 ? `${((keyspace.expires / keyspace.keys) * 100).toFixed(0)}% of keys` : undefined} />
            <StatCard label="Expired keys" icon={<Timer className="size-4" />} value={n("expired_keys")?.toLocaleString() ?? "—"} sub={delta(n("expired_keys"), p("expired_keys"))} />
            <StatCard label="Evicted keys" icon={<Trash2 className="size-4" />} value={n("evicted_keys")?.toLocaleString() ?? "—"} tone={(n("evicted_keys") ?? 0) > 0 ? "warn" : "neutral"} sub={delta(n("evicted_keys"), p("evicted_keys"))} />
            <StatCard label="Network in" icon={<ArrowDownToLine className="size-4" />} value={formatBytes(n("total_net_input_bytes") ?? NaN)} sub={delta(n("total_net_input_bytes"), p("total_net_input_bytes"))?.replace("since last sample", "bytes")} />
            <StatCard label="Network out" icon={<ArrowUpFromLine className="size-4" />} value={formatBytes(n("total_net_output_bytes") ?? NaN)} sub={delta(n("total_net_output_bytes"), p("total_net_output_bytes"))?.replace("since last sample", "bytes")} />
          </div>

          <div className="grid gap-4 lg:grid-cols-2">
            {Object.entries(data.sections).map(([name, values]) => (
              <Card key={name}>
                <CardHeader title={<span className="font-mono"># {name.charAt(0).toUpperCase() + name.slice(1)}</span>} description={`INFO ${name}`} />
                <dl className="divide-y divide-border font-mono text-xs">
                  {Object.entries(values).map(([key, value]) => (
                    <div key={key} className="flex justify-between gap-4 px-4 py-1.5">
                      <dt className="text-muted">{key}</dt>
                      <dd className="truncate text-text">{value}</dd>
                    </div>
                  ))}
                </dl>
              </Card>
            ))}
          </div>

          <Collapsible
            title={
              <>
                Raw INFO output
                {loadedAt && (
                  <span className="ml-2 font-mono text-[11px] font-normal text-faint">
                    sampled {new Date(loadedAt).toLocaleTimeString([], { hour12: false })}
                  </span>
                )}
              </>
            }
            actions={<CopyButton text={data.raw} label="Copy INFO" />}
          >
            <pre className="max-h-96 overflow-auto p-4 font-mono text-xs leading-5 text-muted">{data.raw.replace(/\r\n/g, "\n")}</pre>
          </Collapsible>
        </div>
      )}
    </>
  );
}
