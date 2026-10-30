import { NextResponse } from "next/server";

import { executeCommand, httpStatusFor } from "@/lib/bridge/execute";
import { INFO_SECTIONS, parseInfo } from "@/lib/info";
import { isTarget, type InfoResponse } from "@/lib/resp/types";

export const dynamic = "force-dynamic";

/** GET /api/info?target=primary&section=memory — INFO, parsed into sections. */
export async function GET(request: Request) {
  const params = new URL(request.url).searchParams;
  const target = params.get("target") ?? "primary";
  const section = params.get("section");

  if (!isTarget(target)) {
    return NextResponse.json({ error: 'target must be "primary" or "replica"' }, { status: 400 });
  }

  if (section !== null && !(INFO_SECTIONS as readonly string[]).includes(section.toLowerCase())) {
    return NextResponse.json({ error: `section must be one of ${INFO_SECTIONS.join(", ")}` }, { status: 400 });
  }

  const response = await executeCommand(section ? ["INFO", section] : ["INFO"], target);

  if (response.outcome !== "reply" || typeof response.value !== "string") {
    const body: InfoResponse = {
      ok: false,
      target,
      error: response.outcome === "failure" ? response.error : response.outcome === "redis-error" ? response.redisError : "unexpected INFO reply",
      response,
    };
    return NextResponse.json(body, { status: response.outcome === "failure" ? httpStatusFor(response) : 200 });
  }

  const body: InfoResponse = {
    ok: true,
    target,
    sections: parseInfo(response.value),
    raw: response.value,
    execution: response,
  };
  return NextResponse.json(body);
}
