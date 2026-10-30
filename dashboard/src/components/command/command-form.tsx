"use client";

import { CornerDownLeft } from "lucide-react";
import type { FormEvent } from "react";

import { Button } from "@/components/ui/button";
import { Field, Input } from "@/components/ui/input";
import { cn } from "@/lib/cn";

import { CommandLine } from "./command-line";
import type { CommandSpec } from "./command-spec";

/**
 * A single command card generated from a CommandSpec. Enter in any field
 * submits; the exact command that will be sent is previewed below.
 */
export function CommandForm({
  spec,
  values,
  onChange,
  onRun,
  busy,
}: {
  spec: CommandSpec;
  values: Record<string, string>;
  onChange: (name: string, value: string, shared: boolean) => void;
  onRun: (args: string[]) => void;
  busy: boolean;
}) {
  const args = spec.build(values);
  const missing = spec.fields.some((field) => !field.optional && (values[field.name] ?? "") === "");

  const submit = (event: FormEvent) => {
    event.preventDefault();
    if (!missing) onRun(args);
  };

  return (
    <form onSubmit={submit} className="rounded-lg border border-border bg-panel-2/60 p-3 transition-colors focus-within:border-border-strong">
      <div className="mb-2.5 flex items-baseline justify-between gap-3">
        <div className="flex items-baseline gap-2">
          <h3 className="font-mono text-sm font-semibold text-text">{spec.title}</h3>
          <span
            className={cn(
              "text-[10px] font-medium tracking-wide uppercase",
              spec.kind === "write" ? "text-warn" : "text-faint",
            )}
          >
            {spec.kind ?? "read"}
          </span>
        </div>
        <p className="truncate text-xs text-muted">{spec.description}</p>
      </div>

      <div className="flex flex-wrap items-end gap-2">
        {spec.fields.map((field) => (
          <Field key={field.name} label={field.label} className="min-w-28" style={{ flexGrow: field.grow ?? 1, flexBasis: 0 }}>
            <Input
              value={values[field.name] ?? ""}
              placeholder={field.placeholder}
              inputMode={field.inputMode}
              onChange={(event) => onChange(field.name, event.target.value, field.shared ?? false)}
            />
          </Field>
        ))}
        <Button type="submit" variant={spec.kind === "write" ? "primary" : "secondary"} loading={busy} disabled={missing} icon={<CornerDownLeft className="size-3.5" />}>
          Run
        </Button>
      </div>

      <div className="mt-2 truncate text-[11px] text-faint">
        <span className="mr-1.5 select-none">→</span>
        <CommandLine args={args} className="font-mono text-[11px]" />
      </div>
    </form>
  );
}
