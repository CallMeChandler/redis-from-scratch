"use client";

import { useHealth } from "@/components/providers/health-provider";
import { StatusDot } from "@/components/ui/badge";
import { Card } from "@/components/ui/card";
import { useInfo } from "@/hooks/use-info";
import { cn } from "@/lib/cn";
import { formatLatency } from "@/lib/resp/format";
import type { Target } from "@/lib/resp/types";

/** Health + INFO replication for one server. */
export function EndpointCard({ target }: { target: Target }) {
  const { health } = useHealth();
  const endpoint = health?.[target];
  const configured = endpoint?.status !== "not-configured";
  const { data } = useInfo(target, "replication", configured ? 3000 : 0, "Replication");
  const fields = data?.ok ? (data.sections.replication ?? {}) : {};

  const status = endpoint?.status ?? "offline";
  const label = status === "online" ? "ONLINE" : status === "offline" ? "OFFLINE" : "NOT CONFIGURED";

  return (
    <Card className="overflow-hidden">
      <div className={cn("h-0.5", status === "online" ? "bg-ok" : status === "offline" ? "bg-err" : "bg-border-strong")} />
      <div className="space-y-3 px-4 py-4">
        <div className="flex items-start justify-between gap-3">
          <div>
            <p className="text-[11px] font-semibold tracking-[0.12em] text-muted uppercase">{target}</p>
            <p className="mt-0.5 font-mono text-lg text-text">{endpoint?.host ? `${endpoint.host}:${endpoint.port}` : "—"}</p>
          </div>
          <span className="flex items-center gap-2 font-mono text-xs font-semibold">
            <StatusDot tone={status === "online" ? "ok" : status === "offline" ? "err" : "neutral"} pulse={status === "online"} />
            <span className={status === "online" ? "text-ok" : status === "offline" ? "text-err" : "text-faint"}>{label}</span>
          </span>
        </div>
        <div className="rounded-md border border-border bg-bg px-3 py-2 font-mono text-xs leading-6">
          {Object.keys(fields).length > 0 ? (
            Object.entries(fields).map(([key, value]) => (
              <div key={key}>
                <span className="text-muted">{key}:</span>
                <span
                  className={cn(
                    value === "up" ? "text-ok" : value === "down" ? "text-err" : "text-text",
                  )}
                >
                  {value}
                </span>
              </div>
            ))
          ) : (
            <span className="text-faint">{status === "not-configured" ? "Set REPLICA_HOST / REPLICA_PORT to enable." : endpoint?.error ?? "No INFO replication data."}</span>
          )}
        </div>
        {endpoint?.latencyMs !== undefined && <p className="font-mono text-[11px] text-faint">PING {formatLatency(endpoint.latencyMs)}</p>}
      </div>
    </Card>
  );
}
