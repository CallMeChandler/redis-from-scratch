import type { CommandResponse, HealthResponse, InfoResponse, Target } from "../resp/types";
import { makeId } from "./id";

function unreachable(args: string[], target: Target, message: string): CommandResponse {
  return {
    ok: false,
    outcome: "failure",
    id: makeId(),
    kind: "connection",
    error: `dashboard backend unreachable: ${message}`,
    target,
    via: "http",
    command: args,
    startedAt: Date.now(),
  };
}

/** Runs a stateless command through POST /api/command. Never throws. */
export async function postCommand(args: string[], target: Target = "primary"): Promise<CommandResponse> {
  try {
    const response = await fetch("/api/command", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ target, args }),
    });
    return (await response.json()) as CommandResponse;
  } catch (error) {
    return unreachable(args, target, error instanceof Error ? error.message : String(error));
  }
}

export async function fetchInfo(target: Target, section?: string): Promise<InfoResponse> {
  const params = new URLSearchParams({ target });
  if (section) params.set("section", section);

  try {
    const response = await fetch(`/api/info?${params}`, { cache: "no-store" });
    return (await response.json()) as InfoResponse;
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    return { ok: false, target, error: message, response: unreachable(["INFO"], target, message) };
  }
}

export async function fetchHealth(): Promise<HealthResponse | null> {
  try {
    const response = await fetch("/api/health", { cache: "no-store" });
    return (await response.json()) as HealthResponse;
  } catch {
    return null;
  }
}
