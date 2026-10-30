"use client";

import { CommandPlayground, type SnapshotContext } from "@/components/command/command-playground";
import { keyField, type CommandSpec } from "@/components/command/command-spec";
import { PageHeader } from "@/components/ui/page-header";
import { useSnapshot } from "@/hooks/use-snapshot";
import type { CommandRoundTrip } from "@/lib/resp/types";

import { RankedMembers, SnapshotCard, SnapshotMessage, asStrings, errorOf, replyOf } from "./visuals";

const zKey = { ...keyField, defaultValue: "leaderboard" };
const member = { name: "member", label: "Member", placeholder: "member", shared: true, grow: 2 };
const MAX_SCORED = 40;

const SPECS: CommandSpec[] = [
  {
    id: "zadd",
    title: "ZADD",
    kind: "write",
    description: "Add or update one member's score",
    fields: [zKey, { name: "score", label: "Score", defaultValue: "100", inputMode: "numeric" }, member],
    build: (v) => ["ZADD", v.key, v.score, v.member],
  },
  { id: "zscore", title: "ZSCORE", description: "Score of a member", fields: [zKey, member], build: (v) => ["ZSCORE", v.key, v.member] },
  {
    id: "zrange",
    title: "ZRANGE",
    description: "Members by rank, lowest score first",
    fields: [
      zKey,
      { name: "start", label: "Start", defaultValue: "0", inputMode: "numeric" },
      { name: "stop", label: "Stop", defaultValue: "-1", inputMode: "numeric" },
    ],
    build: (v) => ["ZRANGE", v.key, v.start, v.stop],
  },
];

function visualize(response: CommandRoundTrip) {
  const items = asStrings(response.replies[0]);
  return items ? <RankedMembers members={items} /> : undefined;
}

/** ZRANGE has no WITHSCORES here, so scores come from one ZSCORE per member. */
function ZsetSnapshot({ shared, target, version }: SnapshotContext) {
  const key = shared.key ?? "";
  const range = useSnapshot("Sorted Sets", key ? [["ZRANGE", key, "0", "-1"]] : null, target, version);
  const members = asStrings(replyOf(range?.[0]));
  const shown = members?.slice(0, MAX_SCORED) ?? [];
  const scoreResponses = useSnapshot("Sorted Sets", shown.length ? shown.map((name) => ["ZSCORE", key, name]) : null, target, version);
  const error = errorOf(range?.[0]);

  const scores: Record<string, string> = {};
  shown.forEach((name, index) => {
    const reply = replyOf(scoreResponses?.[index]);
    if (reply?.type === "bulk-string") scores[name] = reply.value;
  });

  return (
    <SnapshotCard title="Live sorted set" keyName={key} meta={members ? `${members.length} member(s)` : undefined}>
      {!key ? (
        <SnapshotMessage>Type a key to watch it.</SnapshotMessage>
      ) : error ? (
        <SnapshotMessage tone="err">{error}</SnapshotMessage>
      ) : members ? (
        <RankedMembers members={shown} scores={scores} />
      ) : (
        <SnapshotMessage>Loading…</SnapshotMessage>
      )}
    </SnapshotCard>
  );
}

export function SortedSetsView() {
  return (
    <>
      <PageHeader
        title="Sorted Sets"
        description="Members ordered by score. ZADD takes one score/member pair; ZRANGE returns members only (no WITHSCORES)."
        commands={["ZADD", "ZSCORE", "ZRANGE"]}
      />
      <CommandPlayground origin="Sorted Sets" specs={SPECS} visualize={visualize} snapshot={(context) => <ZsetSnapshot {...context} />} />
    </>
  );
}
