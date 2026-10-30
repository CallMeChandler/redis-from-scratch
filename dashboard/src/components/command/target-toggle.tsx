"use client";

import { useHealth } from "@/components/providers/health-provider";
import { Segmented } from "@/components/ui/segmented";
import type { Target } from "@/lib/resp/types";

export function TargetToggle({ value, onChange, size = "sm" }: { value: Target; onChange: (target: Target) => void; size?: "sm" | "md" }) {
  const { health } = useHealth();
  const replicaMissing = health?.replica.status === "not-configured";

  return (
    <Segmented
      label="Target server"
      size={size}
      value={value}
      onChange={onChange}
      options={[
        { value: "primary", label: "Primary" },
        {
          value: "replica",
          label: "Replica",
          disabled: replicaMissing,
          title: replicaMissing ? "Set REPLICA_HOST to enable" : undefined,
        },
      ]}
    />
  );
}
