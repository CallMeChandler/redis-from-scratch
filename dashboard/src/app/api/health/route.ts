import { NextResponse } from "next/server";

import { getEndpoint } from "@/lib/bridge/config";
import { executeCommand } from "@/lib/bridge/execute";
import type { EndpointHealth, HealthResponse, Target } from "@/lib/resp/types";

export const dynamic = "force-dynamic";

async function check(target: Target): Promise<EndpointHealth> {
  const endpoint = getEndpoint(target);

  if (endpoint === null) {
    return { target, status: "not-configured", host: null, port: null };
  }

  const response = await executeCommand(["PING"], target);
  const base = { target, host: endpoint.host, port: endpoint.port };

  if (response.outcome === "failure") {
    return { ...base, status: "offline", error: response.error };
  }

  return {
    ...base,
    status: "online",
    latencyMs: response.latencyMs,
    error: response.outcome === "redis-error" ? response.redisError : undefined,
  };
}

/** GET /api/health — PING both targets; never throws when they are down. */
export async function GET() {
  const [primary, replica] = await Promise.all([check("primary"), check("replica")]);
  const body: HealthResponse = { checkedAt: Date.now(), primary, replica };
  return NextResponse.json(body, { headers: { "Cache-Control": "no-store" } });
}
