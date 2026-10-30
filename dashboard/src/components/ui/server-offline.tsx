import { PlugZap } from "lucide-react";

import type { Target } from "@/lib/resp/types";

/** Clean disconnected state with the command to bring the server up. */
export function ServerOffline({ target, error }: { target: Target; error?: string }) {
  const config = target === "primary" ? "examples/primary.conf" : "examples/replica.conf";

  return (
    <div className="flex flex-col items-center gap-3 rounded-xl border border-dashed border-border-strong bg-panel px-6 py-10 text-center">
      <PlugZap className="size-6 text-faint" />
      <div>
        <p className="text-sm font-medium text-text">
          {target === "primary" ? "Primary" : "Replica"} is not reachable
        </p>
        {error && <p className="mt-1 font-mono text-xs text-err">{error}</p>}
      </div>
      <p className="text-xs text-muted">Start it from the repository root:</p>
      <code className="rounded-md border border-border bg-bg px-3 py-1.5 font-mono text-xs text-text">
        ./build/redis-server {config}
      </code>
    </div>
  );
}
