"use client";

import { CommandPlayground } from "@/components/command/command-playground";
import { keyField, type CommandSpec } from "@/components/command/command-spec";
import { PageHeader } from "@/components/ui/page-header";

import { TtlWatcher } from "./ttl-watcher";

const ttlKey = { ...keyField, defaultValue: "session:user" };

/** "+120" → now + 120 s, resolved when the command runs; the server only ever sees the absolute value. */
function resolveTimestamp(input: string): string {
  const relative = /^\+(\d+)$/.exec(input.trim());
  return relative ? String(Math.floor(Date.now() / 1000) + Number(relative[1])) : input;
}

const SPECS: CommandSpec[] = [
  {
    id: "set",
    title: "SET",
    kind: "write",
    description: "Create a key to expire",
    fields: [ttlKey, { name: "value", label: "Value", defaultValue: "active", optional: true }],
    build: (v) => ["SET", v.key, v.value],
  },
  {
    id: "expire",
    title: "EXPIRE",
    kind: "write",
    description: "Relative expiry in seconds (1 = set, 0 = missing key)",
    fields: [ttlKey, { name: "seconds", label: "Seconds", defaultValue: "45", inputMode: "numeric" }],
    build: (v) => ["EXPIRE", v.key, v.seconds],
  },
  {
    id: "expireat",
    title: "EXPIREAT",
    kind: "write",
    description: "Absolute unix seconds; +N is shorthand for now + N",
    fields: [ttlKey, { name: "timestamp", label: "Unix seconds", defaultValue: "+120", placeholder: "1793365428 or +120" }],
    build: (v) => ["EXPIREAT", v.key, v.timestamp],
    prepare: ([name, key, timestamp]) => [name, key, resolveTimestamp(timestamp)],
  },
  {
    id: "ttl",
    title: "TTL",
    description: "-1 = no expiry, -2 = key missing",
    fields: [ttlKey],
    build: (v) => ["TTL", v.key],
  },
];

export function TtlView() {
  return (
    <>
      <PageHeader
        title="TTL"
        description="Expiration uses lazy deletion on access plus an active expiration cycle in the event loop. Watch keys count down and disappear."
        commands={["EXPIRE", "EXPIREAT", "TTL"]}
      />
      <CommandPlayground origin="TTL" specs={SPECS} snapshot={(context) => <TtlWatcher {...context} />} />
    </>
  );
}
