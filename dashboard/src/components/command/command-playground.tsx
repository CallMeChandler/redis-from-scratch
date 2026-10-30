"use client";

import { useState, type ReactNode } from "react";

import { useCommandRunner } from "@/hooks/use-command";
import type { CommandResponse, CommandRoundTrip, Target } from "@/lib/resp/types";

import { CommandForm } from "./command-form";
import type { CommandSpec } from "./command-spec";
import { HistoryList } from "./history-list";
import { ResultCard } from "./result-card";
import { TargetToggle } from "./target-toggle";

export interface SnapshotContext {
  shared: Record<string, string>;
  target: Target;
  /** Increments after every command, so snapshots can re-read the key. */
  version: number;
  /** Commands run on this page, newest first. */
  history: CommandResponse[];
}

const HISTORY_LIMIT = 30;

function initialValues(specs: CommandSpec[], shared: boolean): Record<string, string> {
  const values: Record<string, string> = {};
  for (const spec of specs) {
    for (const field of spec.fields) {
      if ((field.shared ?? false) === shared && field.defaultValue !== undefined) {
        values[shared ? field.name : `${spec.id}.${field.name}`] = field.defaultValue;
      }
    }
  }
  return values;
}

/**
 * Generic page body for data-type pages: a stack of command forms, the
 * latest parsed response, a page-local history and an optional live
 * snapshot of the key being worked on.
 */
export function CommandPlayground({
  origin,
  specs,
  visualize,
  snapshot,
}: {
  origin: string;
  specs: CommandSpec[];
  visualize?: (response: CommandRoundTrip) => ReactNode;
  snapshot?: (context: SnapshotContext) => ReactNode;
}) {
  const { run } = useCommandRunner(origin);
  const [target, setTarget] = useState<Target>("primary");
  const [shared, setShared] = useState(() => initialValues(specs, true));
  const [local, setLocal] = useState(() => initialValues(specs, false));
  const [current, setCurrent] = useState<CommandResponse | null>(null);
  const [history, setHistory] = useState<CommandResponse[]>([]);
  const [busy, setBusy] = useState<string | null>(null);
  const [version, setVersion] = useState(0);

  const valuesFor = (spec: CommandSpec) => {
    const values: Record<string, string> = {};
    for (const field of spec.fields) {
      values[field.name] = (field.shared ? shared[field.name] : local[`${spec.id}.${field.name}`]) ?? "";
    }
    return values;
  };

  const execute = async (spec: CommandSpec, args: string[]) => {
    setBusy(spec.id);
    const response = await run(spec.prepare ? spec.prepare(args) : args, target);
    setBusy(null);
    setCurrent(response);
    setHistory((items) => [response, ...items].slice(0, HISTORY_LIMIT));
    setVersion((value) => value + 1);
  };

  return (
    <div className="grid gap-4 xl:grid-cols-[minmax(0,1fr)_minmax(0,0.9fr)]">
      <div className="min-w-0 space-y-3">
        <div className="flex items-center justify-between gap-3">
          <p className="text-xs text-muted">
            Commands run over the shared stateless socket to the selected server.
          </p>
          <TargetToggle value={target} onChange={setTarget} />
        </div>
        {specs.map((spec) => (
          <CommandForm
            key={spec.id}
            spec={spec}
            values={valuesFor(spec)}
            busy={busy === spec.id}
            onRun={(args) => execute(spec, args)}
            onChange={(name, value, isShared) =>
              isShared
                ? setShared((values) => ({ ...values, [name]: value }))
                : setLocal((values) => ({ ...values, [`${spec.id}.${name}`]: value }))
            }
          />
        ))}
      </div>

      <div className="min-w-0 space-y-4">
        {snapshot?.({ shared, target, version, history })}
        <ResultCard response={current} visualize={visualize} />
        <HistoryList items={history} selectedId={current?.id} onSelect={setCurrent} />
      </div>
    </div>
  );
}
