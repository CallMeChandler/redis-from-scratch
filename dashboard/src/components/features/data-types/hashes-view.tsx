"use client";

import { CommandPlayground, type SnapshotContext } from "@/components/command/command-playground";
import { keyField, type CommandSpec } from "@/components/command/command-spec";
import { PageHeader } from "@/components/ui/page-header";
import { useSnapshot } from "@/hooks/use-snapshot";

import { SnapshotCard, SnapshotMessage, errorOf, replyOf } from "./visuals";

const fieldField = { name: "field", label: "Field", placeholder: "field", shared: true };

const SPECS: CommandSpec[] = [
  {
    id: "hset",
    title: "HSET",
    kind: "write",
    description: "Set one field (returns 1 if new, 0 if updated)",
    fields: [{ ...keyField, defaultValue: "user:1" }, { ...fieldField, defaultValue: "name" }, { name: "value", label: "Value", placeholder: "value", grow: 2, optional: true }],
    build: (v) => ["HSET", v.key, v.field, v.value],
  },
  {
    id: "hget",
    title: "HGET",
    description: "Read one field",
    fields: [keyField, fieldField],
    build: (v) => ["HGET", v.key, v.field],
  },
];

/**
 * The server has no HGETALL/HKEYS, so the live view tracks every field you
 * touch on this page and HGETs each of them.
 */
function HashSnapshot({ shared, target, version, history }: SnapshotContext) {
  const key = shared.key ?? "";
  const tracked = [
    ...new Set(
      history
        .filter((item) => ["HSET", "HGET"].includes(item.command[0]?.toUpperCase()) && item.command[1] === key)
        .map((item) => item.command[2])
        .reverse(),
    ),
  ].slice(-16);
  const responses = useSnapshot("Hashes", key && tracked.length ? tracked.map((name) => ["HGET", key, name]) : null, target, version);

  return (
    <SnapshotCard title="Tracked fields" keyName={key} meta={`${tracked.length} field(s) via HGET`}>
      {!key || tracked.length === 0 ? (
        <SnapshotMessage>Run HSET or HGET on this key to start tracking its fields.</SnapshotMessage>
      ) : (
        <table className="w-full font-mono text-xs">
          <tbody className="divide-y divide-border">
            {tracked.map((name, index) => {
              const reply = replyOf(responses?.[index]);
              const error = errorOf(responses?.[index]);
              return (
                <tr key={name}>
                  <td className="w-1/3 py-1.5 pr-3 text-muted">{name}</td>
                  <td className="py-1.5 break-all">
                    {error ? (
                      <span className="text-err">{error}</span>
                    ) : reply?.type === "bulk-string" ? (
                      <span className="text-t-string">&quot;{reply.value}&quot;</span>
                    ) : (
                      <span className="text-t-null italic">(nil)</span>
                    )}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      )}
    </SnapshotCard>
  );
}

export function HashesView() {
  return (
    <>
      <PageHeader
        title="Hashes"
        description="HSET writes a single field/value pair per call. Change the field name and run again to build up a hash."
        commands={["HSET", "HGET"]}
      />
      <CommandPlayground origin="Hashes" specs={SPECS} snapshot={(context) => <HashSnapshot {...context} />} />
    </>
  );
}
