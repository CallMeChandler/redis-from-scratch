"use client";

import { Menu, RefreshCw } from "lucide-react";

import { useHealth } from "@/components/providers/health-provider";
import { StatusDot, type Tone } from "@/components/ui/badge";
import { useBridgeStatus } from "@/hooks/use-session";
import { cn } from "@/lib/cn";
import { formatLatency } from "@/lib/resp/format";
import type { EndpointHealth, HealthStatus } from "@/lib/resp/types";

import { ThemeToggle } from "./theme-toggle";

const STATUS: Record<HealthStatus, { tone: Tone; label: string }> = {
  online: { tone: "ok", label: "ONLINE" },
  offline: { tone: "err", label: "OFFLINE" },
  "not-configured": { tone: "neutral", label: "NOT CONFIGURED" },
};

function Endpoint({ name, health }: { name: string; health: EndpointHealth | undefined }) {
  const status = health ? STATUS[health.status] : { tone: "neutral" as Tone, label: "CHECKING" };

  return (
    <div
      className="flex items-center gap-2 rounded-md border border-border bg-panel-2 px-2.5 py-1"
      title={health?.error ?? (health?.latencyMs !== undefined ? `PING ${formatLatency(health.latencyMs)}` : undefined)}
    >
      <StatusDot tone={status.tone} pulse={health?.status === "online"} />
      <span className="text-[11px] font-medium text-muted">{name}</span>
      <span
        className={cn(
          "font-mono text-[11px] font-semibold",
          status.tone === "ok" ? "text-ok" : status.tone === "err" ? "text-err" : "text-faint",
        )}
      >
        {status.label}
      </span>
      {health?.host && (
        <span className="hidden font-mono text-[11px] text-faint lg:inline">
          {health.host}:{health.port}
        </span>
      )}
    </div>
  );
}

export function TopBar({ onMenu }: { onMenu: () => void }) {
  const { health, checking, refresh } = useHealth();
  const bridge = useBridgeStatus();

  return (
    <header className="sticky top-0 z-40 flex h-12 shrink-0 items-center gap-3 border-b border-border bg-panel/95 px-3 backdrop-blur md:px-4">
      <button
        type="button"
        onClick={onMenu}
        className="rounded-md p-1.5 text-muted hover:bg-hover hover:text-text md:hidden"
        aria-label="Toggle navigation"
      >
        <Menu className="size-4" />
      </button>

      <div className="flex items-center gap-2">
        <span className="grid size-6 place-items-center rounded-md bg-brand/15 font-mono text-[11px] font-bold text-brand">R</span>
        <span className="text-sm font-semibold tracking-tight text-text">Redis From Scratch</span>
        <span className="hidden rounded border border-border px-1.5 py-px font-mono text-[10px] text-faint sm:inline">RESP2</span>
      </div>

      <div className="ml-auto flex items-center gap-2">
        <div className="hidden items-center gap-2 sm:flex">
          <Endpoint name="Primary" health={health?.primary} />
          <Endpoint name="Replica" health={health?.replica} />
        </div>
        <div className="flex items-center gap-2.5 font-mono text-[10px] text-muted sm:hidden">
          {(["primary", "replica"] as const).map((target) => (
            <span key={target} className="flex items-center gap-1" title={`${target}: ${health?.[target].status ?? "checking"}`}>
              <StatusDot tone={health ? STATUS[health[target].status].tone : "neutral"} />
              {target === "primary" ? "P" : "R"}
            </span>
          ))}
        </div>
        <span
          className="hidden items-center gap-1.5 px-1 font-mono text-[10px] text-faint xl:flex"
          title="WebSocket between this tab and the dashboard backend (used by sessions)"
        >
          <StatusDot tone={bridge === "open" ? "ok" : bridge === "connecting" ? "warn" : "neutral"} />
          ws {bridge === "open" ? "live" : bridge === "connecting" ? "connecting" : "idle"}
        </span>
        <button
          type="button"
          onClick={refresh}
          className="rounded-md p-1.5 text-muted transition-colors hover:bg-hover hover:text-text"
          aria-label="Refresh"
          title="Re-check health and reload page data"
        >
          <RefreshCw className={cn("size-4", checking && "animate-spin")} />
        </button>
        <ThemeToggle />
      </div>
    </header>
  );
}
