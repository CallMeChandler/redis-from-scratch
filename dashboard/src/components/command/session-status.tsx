"use client";

import { RotateCcw } from "lucide-react";

import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import type { SessionStatus } from "@/lib/client/bridge-socket";

/** Status line for a dedicated session socket, with a reconnect action. */
export function SessionStatusBar({
  status,
  address,
  error,
  onReconnect,
  label = "Session socket",
}: {
  status: SessionStatus;
  address: string;
  error: string | null;
  onReconnect: () => void;
  label?: string;
}) {
  const tone = status === "open" ? "ok" : status === "connecting" ? "warn" : "err";

  return (
    <div className="flex flex-wrap items-center gap-2 rounded-lg border border-border bg-panel-2 px-3 py-2 text-xs">
      <Badge tone={tone}>{status === "open" ? "connected" : status}</Badge>
      <span className="text-muted">{label}</span>
      {address && <code className="font-mono text-faint">{address}</code>}
      {error && status !== "open" && <span className="min-w-0 truncate font-mono text-err">{error}</span>}
      {status !== "open" && status !== "connecting" && (
        <Button size="sm" variant="ghost" className="ml-auto" icon={<RotateCcw className="size-3.5" />} onClick={onReconnect}>
          Reconnect
        </Button>
      )}
    </div>
  );
}
