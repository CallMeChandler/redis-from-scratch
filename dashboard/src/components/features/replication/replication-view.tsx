"use client";

import { ArrowRight, BookOpen, PenLine, ShieldAlert, Timer } from "lucide-react";
import { useState } from "react";

import { ResultCard } from "@/components/command/result-card";
import { useHealth } from "@/components/providers/health-provider";
import { Button } from "@/components/ui/button";
import { Card, CardBody, CardHeader } from "@/components/ui/card";
import { Field, Input } from "@/components/ui/input";
import { PageHeader } from "@/components/ui/page-header";
import { useCommandRunner } from "@/hooks/use-command";
import type { CommandResponse } from "@/lib/resp/types";

import { EndpointCard } from "./endpoint-card";

const POLL_MS = 25;
const POLL_TIMEOUT_MS = 3000;

export function ReplicationView() {
  const { health } = useHealth();
  const { run } = useCommandRunner("Replication");
  const [key, setKey] = useState("replication:test");
  const [value, setValue] = useState("hello");
  const [write, setWrite] = useState<CommandResponse | null>(null);
  const [read, setRead] = useState<CommandResponse | null>(null);
  const [readonly, setReadonly] = useState<CommandResponse | null>(null);
  const [propagation, setPropagation] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const replicaReady = health?.replica.status === "online";

  const writePrimary = async () => setWrite(await run(["SET", key, value], "primary"));
  const readReplica = async () => setRead(await run(["GET", key], "replica"));

  /** Writes a unique value, then polls the replica until it appears. */
  const measure = async () => {
    setBusy(true);
    setPropagation(null);
    const unique = `${value}@${Date.now()}`;
    const started = performance.now();
    setWrite(await run(["SET", key, unique], "primary"));

    let response: CommandResponse;
    let polls = 0;
    do {
      response = await run(["GET", key], "replica", { auto: true });
      polls += 1;
      const reply = response.outcome === "reply" ? response.replies[0] : null;
      if (reply?.type === "bulk-string" && reply.value === unique) break;
      await new Promise((resolve) => setTimeout(resolve, POLL_MS));
    } while (performance.now() - started < POLL_TIMEOUT_MS && response.outcome !== "failure");

    const elapsed = performance.now() - started;
    const arrived = response.outcome === "reply" && response.replies[0].type === "bulk-string" && response.replies[0].value === unique;
    setRead(response);
    setPropagation(arrived ? `visible on replica after ${elapsed.toFixed(1)} ms (${polls} GET${polls === 1 ? "" : "s"})` : `not visible on replica after ${POLL_TIMEOUT_MS} ms`);
    setBusy(false);
  };

  return (
    <>
      <PageHeader
        title="Replication"
        description="The replica performs a full sync on connect, then applies the primary's write stream. Ordinary clients cannot write to it."
        commands={["INFO replication", "SET → primary", "GET ← replica"]}
      />

      <div className="grid gap-4 md:grid-cols-2">
        <EndpointCard target="primary" />
        <EndpointCard target="replica" />
      </div>

      <Card className="mt-4">
        <CardHeader title="Write to primary, read from replica" description="Each side uses its own socket to its own server" />
        <CardBody className="space-y-4">
          <div className="flex flex-wrap items-end gap-2">
            <Field label="Key" className="min-w-44 flex-1">
              <Input value={key} onChange={(event) => setKey(event.target.value)} />
            </Field>
            <Field label="Value" className="min-w-32 flex-1">
              <Input value={value} onChange={(event) => setValue(event.target.value)} />
            </Field>
            <Button variant="primary" icon={<PenLine className="size-3.5" />} onClick={writePrimary} disabled={!key}>
              Write to Primary
            </Button>
            <Button icon={<BookOpen className="size-3.5" />} onClick={readReplica} disabled={!key || !replicaReady}>
              Read From Replica
            </Button>
            <Button variant="ghost" icon={<Timer className="size-3.5" />} onClick={measure} loading={busy} disabled={!key || !replicaReady}>
              Measure propagation
            </Button>
          </div>
          {propagation && <p className="font-mono text-xs text-muted">↳ {propagation}</p>}
          <div className="grid items-start gap-4 lg:grid-cols-[minmax(0,1fr)_auto_minmax(0,1fr)]">
            <ResultCard response={write} title="Primary · SET" emptyText="Write a value to the primary." />
            <ArrowRight className="mt-12 hidden size-5 text-faint lg:block" />
            <ResultCard response={read} title="Replica · GET" emptyText="Then read it back from the replica." />
          </div>
        </CardBody>
      </Card>

      <Card className="mt-4">
        <CardHeader
          icon={<ShieldAlert className="size-4" />}
          title="Replica is read-only"
          description="A write sent to the replica must be rejected with a READONLY error. Here that error is the expected, correct result."
          actions={
            <Button variant="danger" onClick={async () => setReadonly(await run(["SET", key, value], "replica"))} disabled={!replicaReady}>
              Try SET on replica
            </Button>
          }
        />
        <CardBody>
          <ResultCard response={readonly} title="Replica · SET" expectError emptyText="Expected: (error) READONLY You can't write against a read only replica." />
        </CardBody>
      </Card>
    </>
  );
}
