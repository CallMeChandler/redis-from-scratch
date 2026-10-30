"use client";

import { CommandPlayground, type SnapshotContext } from "@/components/command/command-playground";
import { keyField, type CommandSpec } from "@/components/command/command-spec";
import { PageHeader } from "@/components/ui/page-header";
import { useSnapshot } from "@/hooks/use-snapshot";
import { formatBytes } from "@/lib/resp/format";

import { SnapshotCard, SnapshotMessage, errorOf, replyOf } from "./visuals";

const SPECS: CommandSpec[] = [
  {
    id: "set",
    title: "SET",
    kind: "write",
    description: "Store a string value",
    fields: [keyField, { name: "value", label: "Value", placeholder: "value", grow: 3, optional: true }],
    build: (v) => ["SET", v.key, v.value],
  },
  { id: "get", title: "GET", description: "Read a string value", fields: [keyField], build: (v) => ["GET", v.key] },
  {
    id: "exists",
    title: "EXISTS",
    description: "Count how many of the keys exist (space separated)",
    fields: [{ ...keyField, label: "Keys" }],
    build: (v) => ["EXISTS", ...splitKeys(v.key)],
  },
  {
    id: "del",
    title: "DEL",
    kind: "write",
    description: "Delete keys (space separated)",
    fields: [{ ...keyField, label: "Keys" }],
    build: (v) => ["DEL", ...splitKeys(v.key)],
  },
  {
    id: "memory",
    title: "MEMORY USAGE",
    description: "Logical bytes accounted to the key",
    fields: [keyField],
    build: (v) => ["MEMORY", "USAGE", v.key],
  },
];

function splitKeys(text: string): string[] {
  const keys = text.trim().split(/\s+/).filter(Boolean);
  return keys.length > 0 ? keys : [""];
}

function StringSnapshot({ shared, target, version }: SnapshotContext) {
  const key = splitKeys(shared.key ?? "")[0];
  const responses = useSnapshot("Strings", key ? [["GET", key], ["MEMORY", "USAGE", key], ["TTL", key]] : null, target, version);

  const value = replyOf(responses?.[0]);
  const memory = replyOf(responses?.[1]);
  const ttl = replyOf(responses?.[2]);
  const error = errorOf(responses?.[0]);

  return (
    <SnapshotCard
      title="Live key"
      keyName={key}
      meta={
        <>
          {memory?.type === "integer" && <span>{formatBytes(memory.value)}</span>}
          {ttl?.type === "integer" && <span className="ml-3">ttl {ttl.value === -1 ? "none" : ttl.value === -2 ? "—" : `${ttl.value}s`}</span>}
        </>
      }
    >
      {!key ? (
        <SnapshotMessage>Type a key to watch it.</SnapshotMessage>
      ) : error ? (
        <SnapshotMessage tone="err">{error}</SnapshotMessage>
      ) : value?.type === "bulk-string" ? (
        <div className="space-y-1">
          <p className="font-mono text-sm break-all text-t-string">&quot;{value.value}&quot;</p>
          <p className="font-mono text-[11px] text-faint">{new TextEncoder().encode(value.value).length} bytes</p>
        </div>
      ) : (
        <SnapshotMessage>(nil) — key does not exist</SnapshotMessage>
      )}
    </SnapshotCard>
  );
}

export function StringsView() {
  return (
    <>
      <PageHeader
        title="Strings"
        description="The server's SET takes exactly a key and a value (no EX/NX options). EXISTS and DEL accept several keys."
        commands={["SET", "GET", "EXISTS", "DEL", "MEMORY USAGE"]}
      />
      <CommandPlayground origin="Strings" specs={SPECS} snapshot={(context) => <StringSnapshot {...context} />} />
    </>
  );
}
