import { NextResponse } from "next/server";

import { executeCommand, failure, httpStatusFor } from "@/lib/bridge/execute";
import { isTarget } from "@/lib/resp/types";

export const dynamic = "force-dynamic";

/**
 * POST /api/command  { "target": "primary" | "replica", "args": ["SET", "k", "v"] }
 *
 * Redis error replies come back as HTTP 200 with outcome "redis-error";
 * HTTP 4xx/5xx are reserved for bad requests and network failures.
 */
export async function POST(request: Request) {
  let body: unknown;

  try {
    body = await request.json();
  } catch {
    const response = failure({ target: "primary", via: "http" }, [], "bad-request", "body must be JSON");
    return NextResponse.json(response, { status: 400 });
  }

  const { target = "primary", args } = (body ?? {}) as { target?: unknown; args?: unknown };

  if (!isTarget(target)) {
    const response = failure({ target: "primary", via: "http" }, [], "bad-request", 'target must be "primary" or "replica"');
    return NextResponse.json(response, { status: 400 });
  }

  const response = await executeCommand(args as string[], target);
  return NextResponse.json(response, { status: httpStatusFor(response) });
}
