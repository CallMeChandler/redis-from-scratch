"use client";

import { CommandPlayground, type SnapshotContext } from "@/components/command/command-playground";
import { keyField, type CommandSpec } from "@/components/command/command-spec";
import { PageHeader } from "@/components/ui/page-header";
import { useSnapshot } from "@/hooks/use-snapshot";
import type { CommandRoundTrip } from "@/lib/resp/types";

import { ListCells, SnapshotCard, SnapshotMessage, asStrings, errorOf, replyOf } from "./visuals";

const listKey = { ...keyField, defaultValue: "queue" };

const SPECS: CommandSpec[] = [
  {
    id: "lpush",
    title: "LPUSH",
    kind: "write",
    description: "Prepend one value at the head",
    fields: [listKey, { name: "value", label: "Value", placeholder: "value", grow: 2, optional: true }],
    build: (v) => ["LPUSH", v.key, v.value],
  },
  {
    id: "rpush",
    title: "RPUSH",
    kind: "write",
    description: "Append one value at the tail",
    fields: [listKey, { name: "value", label: "Value", placeholder: "value", grow: 2, optional: true }],
    build: (v) => ["RPUSH", v.key, v.value],
  },
  { id: "lpop", title: "LPOP", kind: "write", description: "Remove and return the head", fields: [listKey], build: (v) => ["LPOP", v.key] },
  { id: "rpop", title: "RPOP", kind: "write", description: "Remove and return the tail", fields: [listKey], build: (v) => ["RPOP", v.key] },
  {
    id: "lrange",
    title: "LRANGE",
    description: "Inclusive range; negative indices count from the tail",
    fields: [
      listKey,
      { name: "start", label: "Start", defaultValue: "0", inputMode: "numeric" },
      { name: "stop", label: "Stop", defaultValue: "-1", inputMode: "numeric" },
    ],
    build: (v) => ["LRANGE", v.key, v.start, v.stop],
  },
  { id: "llen", title: "LLEN", description: "Length of the list", fields: [listKey], build: (v) => ["LLEN", v.key] },
];

function visualize(response: CommandRoundTrip) {
  const items = asStrings(response.replies[0]);
  return items ? (
    <div className="rounded-md border border-border bg-bg p-3">
      <ListCells items={items} />
    </div>
  ) : undefined;
}

function ListSnapshot({ shared, target, version }: SnapshotContext) {
  const key = shared.key ?? "";
  const responses = useSnapshot("Lists", key ? [["LRANGE", key, "0", "-1"], ["LLEN", key]] : null, target, version);
  const items = asStrings(replyOf(responses?.[0]));
  const length = replyOf(responses?.[1]);
  const error = errorOf(responses?.[0]);

  return (
    <SnapshotCard title="Live list" keyName={key} meta={length?.type === "integer" ? `LLEN ${length.value}` : undefined}>
      {!key ? (
        <SnapshotMessage>Type a key to watch it.</SnapshotMessage>
      ) : error ? (
        <SnapshotMessage tone="err">{error}</SnapshotMessage>
      ) : items ? (
        <div className="space-y-2">
          <div className="flex justify-between font-mono text-[10px] tracking-wide text-faint uppercase">
            <span>← head (LPUSH / LPOP)</span>
            <span>tail (RPUSH / RPOP) →</span>
          </div>
          <ListCells items={items} />
        </div>
      ) : (
        <SnapshotMessage>Loading…</SnapshotMessage>
      )}
    </SnapshotCard>
  );
}

export function ListsView() {
  return (
    <>
      <PageHeader
        title="Lists"
        description="Push and pop at either end. LPUSH/RPUSH take exactly one value per call on this server."
        commands={["LPUSH", "RPUSH", "LPOP", "RPOP", "LRANGE", "LLEN"]}
      />
      <CommandPlayground origin="Lists" specs={SPECS} visualize={visualize} snapshot={(context) => <ListSnapshot {...context} />} />
    </>
  );
}
