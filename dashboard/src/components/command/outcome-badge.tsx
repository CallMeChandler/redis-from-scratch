import { Badge } from "@/components/ui/badge";
import type { CommandResponse } from "@/lib/resp/types";

export function OutcomeBadge({ response, expectError = false }: { response: CommandResponse; expectError?: boolean }) {
  if (response.outcome === "reply") return <Badge tone="ok">reply</Badge>;
  if (response.outcome === "redis-error") return <Badge tone={expectError ? "warn" : "err"}>{expectError ? "expected error" : "redis error"}</Badge>;
  return <Badge tone="err">{response.kind}</Badge>;
}
