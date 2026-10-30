"use client";

import { Check, X } from "lucide-react";

import { CommandPlayground, type SnapshotContext } from "@/components/command/command-playground";
import { keyField, type CommandSpec } from "@/components/command/command-spec";
import { PageHeader } from "@/components/ui/page-header";
import { useSnapshot } from "@/hooks/use-snapshot";
import type { CommandRoundTrip } from "@/lib/resp/types";

import { MemberChips, SnapshotCard, SnapshotMessage, asStrings, errorOf, replyOf } from "./visuals";

const setKey = { ...keyField, defaultValue: "tags" };
const member = { name: "member", label: "Member", placeholder: "member", shared: true, grow: 2 };

const SPECS: CommandSpec[] = [
  { id: "sadd", title: "SADD", kind: "write", description: "Add one member (1 = added, 0 = already present)", fields: [setKey, member], build: (v) => ["SADD", v.key, v.member] },
  { id: "srem", title: "SREM", kind: "write", description: "Remove one member", fields: [setKey, member], build: (v) => ["SREM", v.key, v.member] },
  { id: "sismember", title: "SISMEMBER", description: "Membership test", fields: [setKey, member], build: (v) => ["SISMEMBER", v.key, v.member] },
  { id: "smembers", title: "SMEMBERS", description: "All members (unordered)", fields: [setKey], build: (v) => ["SMEMBERS", v.key] },
];

function visualize(response: CommandRoundTrip) {
  const reply = response.replies[0];
  const name = response.command[0].toUpperCase();

  if (name === "SISMEMBER" && reply.type === "integer") {
    const yes = reply.value === 1;
    return (
      <div className={`flex items-center gap-3 rounded-md border px-3 py-2.5 ${yes ? "border-ok/30 bg-ok-soft text-ok" : "border-border bg-bg text-muted"}`}>
        {yes ? <Check className="size-4" /> : <X className="size-4" />}
        <span className="font-mono text-sm">
          {response.command[2]} {yes ? "is a member" : "is not a member"} of {response.command[1]}
        </span>
        <span className="ml-auto font-mono text-xs text-t-integer">(integer) {reply.value}</span>
      </div>
    );
  }

  const items = asStrings(reply);
  return items ? (
    <div className="rounded-md border border-border bg-bg p-3">
      <MemberChips items={items} />
    </div>
  ) : undefined;
}

function SetSnapshot({ shared, target, version }: SnapshotContext) {
  const key = shared.key ?? "";
  const responses = useSnapshot("Sets", key ? [["SMEMBERS", key]] : null, target, version);
  const items = asStrings(replyOf(responses?.[0]));
  const error = errorOf(responses?.[0]);

  return (
    <SnapshotCard title="Live set" keyName={key} meta={items ? `${items.length} member(s)` : undefined}>
      {!key ? (
        <SnapshotMessage>Type a key to watch it.</SnapshotMessage>
      ) : error ? (
        <SnapshotMessage tone="err">{error}</SnapshotMessage>
      ) : items ? (
        <MemberChips items={items} highlight={shared.member} />
      ) : (
        <SnapshotMessage>Loading…</SnapshotMessage>
      )}
    </SnapshotCard>
  );
}

export function SetsView() {
  return (
    <>
      <PageHeader
        title="Sets"
        description="Unordered unique members. SADD and SREM take one member per call on this server."
        commands={["SADD", "SREM", "SISMEMBER", "SMEMBERS"]}
      />
      <CommandPlayground origin="Sets" specs={SPECS} visualize={visualize} snapshot={(context) => <SetSnapshot {...context} />} />
    </>
  );
}
