"use client";

import { Eye, EyeOff, Play, Plus, Swords, Trash2, Zap } from "lucide-react";
import { useState, type FormEvent, type ReactNode } from "react";

import { CommandLine } from "@/components/command/command-line";
import { RespValueView } from "@/components/command/resp-value-view";
import { SessionStatusBar } from "@/components/command/session-status";
import { useActivity } from "@/components/providers/activity-provider";
import { useToast } from "@/components/providers/toast-provider";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Card, CardBody, CardHeader } from "@/components/ui/card";
import { EmptyState } from "@/components/ui/empty-state";
import { Field, Input } from "@/components/ui/input";
import { PageHeader } from "@/components/ui/page-header";
import { useCommandRunner } from "@/hooks/use-command";
import { useSession } from "@/hooks/use-session";
import { cn } from "@/lib/cn";
import { formatLatency, summarize } from "@/lib/resp/format";
import { TokenizeError, tokenize } from "@/lib/resp/tokenize";
import type { CommandResponse, RespValue } from "@/lib/resp/types";

const TEMPLATES = ["SET account 10", "SET status active", "EXPIRE account 60", "GET account", "RPUSH audit deposit", "LPOP missing:list"];

interface ExecOutcome {
  queued: string[][];
  response: CommandResponse;
}

function replyOf(response: CommandResponse): RespValue | null {
  return response.outcome === "failure" ? null : response.replies[0];
}

export function TransactionsView() {
  const session = useSession({ target: "primary", kind: "transaction", origin: "Transactions" });
  const { run } = useCommandRunner("Transactions · other client");
  const { inspect } = useActivity();
  const notify = useToast();

  const [watchKeys, setWatchKeys] = useState("account");
  const [draft, setDraft] = useState("");
  const [timeline, setTimeline] = useState<CommandResponse[]>([]);
  const [queued, setQueued] = useState<string[][]>([]);
  const [lastExec, setLastExec] = useState<ExecOutcome | null>(null);
  const [conflictKey, setConflictKey] = useState("account");
  const [conflictValue, setConflictValue] = useState("999");
  const [busy, setBusy] = useState(false);

  const { inMulti, watching } = session.state;
  const open = session.status === "open";

  const send = async (args: string[]) => {
    setBusy(true);
    const response = await session.execute(args);
    setBusy(false);
    setTimeline((items) => [response, ...items].slice(0, 60));

    const name = args[0].toUpperCase();
    const reply = replyOf(response);

    if (name === "MULTI" && response.outcome === "reply") setQueued([]);
    if (reply?.type === "simple-string" && reply.value === "QUEUED") setQueued((items) => [...items, args]);
    if ((name === "EXEC" || name === "DISCARD") && response.outcome === "reply") {
      if (name === "EXEC") setLastExec({ queued, response });
      setQueued([]);
    }
    if (response.outcome === "failure") notify({ tone: "error", title: "Session command failed", description: response.error });
    return response;
  };

  const queue = (event: FormEvent) => {
    event.preventDefault();
    try {
      const args = tokenize(draft);
      if (args.length === 0) return;
      void send(args);
      setDraft("");
    } catch (error) {
      notify({ tone: "warning", title: "Could not parse command", description: error instanceof TokenizeError ? error.message : String(error) });
    }
  };

  const simulateConflict = async () => {
    const response = await run(["SET", conflictKey, conflictValue]);
    if (response.outcome === "reply") {
      notify({
        tone: "info",
        title: `Another client wrote ${conflictKey}`,
        description: watching.includes(conflictKey) ? "This key is WATCHed: EXEC will now return a null array." : "This key is not watched by the session.",
      });
    }
  };

  return (
    <>
      <PageHeader
        title="Transactions"
        description="Every command on this page goes through one dedicated TCP connection held by the Node bridge, exactly like a real client running MULTI/EXEC."
        commands={["WATCH", "MULTI", "EXEC", "DISCARD", "UNWATCH"]}
      />

      <div className="grid gap-4 xl:grid-cols-[minmax(0,1fr)_minmax(0,1fr)]">
        <div className="min-w-0 space-y-4">
          <SessionStatusBar status={session.status} address={session.address} error={session.error} onReconnect={session.reopen} label="Transaction socket → primary" />

          <Card>
            <CardHeader
              title="Build a transaction"
              description="WATCH → MULTI → queue → EXEC or DISCARD"
              actions={
                <div className="flex items-center gap-1.5">
                  {watching.length > 0 && (
                    <Badge tone="accent">
                      <Eye className="size-3" /> watching {watching.join(", ")}
                    </Badge>
                  )}
                  <Badge tone={inMulti ? "warn" : "neutral"}>{inMulti ? "multi · queuing" : "no transaction"}</Badge>
                </div>
              }
            />
            <CardBody className="space-y-4">
              <Step n={1} title="Optimistic lock (optional)">
                <div className="flex flex-wrap items-end gap-2">
                  <Field label="Keys" className="min-w-40 flex-1">
                    <Input value={watchKeys} onChange={(event) => setWatchKeys(event.target.value)} placeholder="space separated" />
                  </Field>
                  <Button icon={<Eye className="size-3.5" />} disabled={!open || inMulti || !watchKeys.trim()} loading={busy} onClick={() => send(["WATCH", ...watchKeys.trim().split(/\s+/)])}>
                    WATCH
                  </Button>
                  <Button variant="ghost" icon={<EyeOff className="size-3.5" />} disabled={!open} onClick={() => send(["UNWATCH"])}>
                    UNWATCH
                  </Button>
                </div>
              </Step>

              <Step n={2} title="Start the transaction">
                <Button variant="primary" icon={<Zap className="size-3.5" />} disabled={!open || inMulti} onClick={() => send(["MULTI"])}>
                  MULTI
                </Button>
              </Step>

              <Step n={3} title={inMulti ? "Queue commands (server replies QUEUED)" : "Commands (run immediately outside MULTI)"}>
                <form onSubmit={queue} className="flex gap-2">
                  <Input value={draft} onChange={(event) => setDraft(event.target.value)} placeholder='SET account 10   ·   quotes: SET msg "hello world"' disabled={!open} />
                  <Button type="submit" icon={<Plus className="size-3.5" />} disabled={!open || !draft.trim()}>
                    {inMulti ? "Queue" : "Run"}
                  </Button>
                </form>
                <div className="mt-2 flex flex-wrap gap-1">
                  {TEMPLATES.map((template) => (
                    <button
                      key={template}
                      type="button"
                      onClick={() => setDraft(template)}
                      className="rounded border border-border bg-panel-2 px-1.5 py-0.5 font-mono text-[11px] text-muted hover:border-border-strong hover:text-text"
                    >
                      {template}
                    </button>
                  ))}
                </div>

                <div className="mt-3 rounded-lg border border-border bg-bg">
                  <p className="border-b border-border px-3 py-1.5 text-[11px] font-medium tracking-wide text-faint uppercase">Queued commands</p>
                  {queued.length === 0 ? (
                    <p className="px-3 py-3 font-mono text-xs text-faint">{inMulti ? "Nothing queued yet." : "Run MULTI to start queuing."}</p>
                  ) : (
                    <ol className="divide-y divide-border">
                      {queued.map((args, index) => (
                        <li key={index} className="flex items-center gap-3 px-3 py-1.5">
                          <span className="w-5 text-right font-mono text-xs text-faint">{index + 1}.</span>
                          <CommandLine args={args} className="min-w-0 flex-1 truncate font-mono text-xs" />
                          <Badge tone="warn">queued</Badge>
                        </li>
                      ))}
                    </ol>
                  )}
                </div>
              </Step>

              <Step n={4} title="Commit or abort">
                <div className="flex gap-2">
                  <Button variant="success" icon={<Play className="size-3.5" />} disabled={!open || !inMulti} onClick={() => send(["EXEC"])}>
                    EXEC
                  </Button>
                  <Button variant="danger" icon={<Trash2 className="size-3.5" />} disabled={!open || !inMulti} onClick={() => send(["DISCARD"])}>
                    DISCARD
                  </Button>
                </div>
              </Step>
            </CardBody>
          </Card>

          <Card>
            <CardHeader
              icon={<Swords className="size-4" />}
              title="Simulate a concurrent client"
              description="SET a key over a different socket (the shared HTTP connection). If the key is WATCHed, the next EXEC aborts."
            />
            <CardBody className="flex flex-wrap items-end gap-2">
              <Field label="Key" className="min-w-32 flex-1">
                <Input value={conflictKey} onChange={(event) => setConflictKey(event.target.value)} />
              </Field>
              <Field label="Value" className="min-w-24 flex-1">
                <Input value={conflictValue} onChange={(event) => setConflictValue(event.target.value)} />
              </Field>
              <Button onClick={simulateConflict} disabled={!conflictKey}>
                SET from other client
              </Button>
            </CardBody>
          </Card>
        </div>

        <div className="min-w-0 space-y-4">
          <ExecResult outcome={lastExec} />

          <Card>
            <CardHeader title="Session timeline" description="Every reply on the transaction socket, newest first" />
            {timeline.length === 0 ? (
              <EmptyState title="No commands on this session yet" description="Start with WATCH or MULTI." />
            ) : (
              <ul className="max-h-[32rem] divide-y divide-border overflow-auto">
                {timeline.map((item) => {
                  const reply = replyOf(item);
                  const isQueued = reply?.type === "simple-string" && reply.value === "QUEUED";
                  return (
                    <li key={item.id}>
                      <button type="button" onClick={() => inspect(item)} className="flex w-full items-center gap-3 px-4 py-1.5 text-left hover:bg-hover">
                        <CommandLine args={item.command} className="min-w-0 flex-1 truncate font-mono text-xs" />
                        <span
                          className={cn(
                            "max-w-[45%] truncate font-mono text-[11px]",
                            item.outcome === "failure" || item.outcome === "redis-error" ? "text-err" : isQueued ? "text-warn" : "text-t-simple",
                          )}
                        >
                          {item.outcome === "failure" ? item.error : item.outcome === "redis-error" ? item.redisError : reply ? summarize(reply) : ""}
                        </span>
                        {item.outcome !== "failure" && <span className="w-14 text-right font-mono text-[11px] text-faint">{formatLatency(item.latencyMs)}</span>}
                      </button>
                    </li>
                  );
                })}
              </ul>
            )}
          </Card>
        </div>
      </div>
    </>
  );
}

function Step({ n, title, children }: { n: number; title: string; children: ReactNode }) {
  return (
    <div className="flex gap-3">
      <span className="mt-0.5 grid size-5 shrink-0 place-items-center rounded-full border border-border-strong font-mono text-[10px] text-muted">{n}</span>
      <div className="min-w-0 flex-1">
        <p className="mb-2 text-xs font-medium text-muted">{title}</p>
        {children}
      </div>
    </div>
  );
}

function ExecResult({ outcome }: { outcome: ExecOutcome | null }) {
  const { inspect } = useActivity();

  if (outcome === null) {
    return (
      <Card>
        <CardHeader title="EXEC result" />
        <EmptyState title="No transaction executed yet" description="EXEC returns one reply per queued command, or a null array if a WATCHed key changed." />
      </Card>
    );
  }

  const reply = replyOf(outcome.response);

  return (
    <Card className={cn(reply?.type === "null-array" && "border-warn/40")}>
      <CardHeader
        title="EXEC result"
        description={outcome.response.outcome !== "failure" ? `${formatLatency(outcome.response.latencyMs)} · ${outcome.queued.length} queued command(s)` : undefined}
        actions={
          <Button size="sm" variant="ghost" onClick={() => inspect(outcome.response)}>
            Inspect
          </Button>
        }
      />
      <CardBody>
        {reply?.type === "null-array" ? (
          <div className="rounded-md border border-warn/30 bg-warn-soft px-3 py-3">
            <p className="text-sm font-medium text-warn">Transaction aborted — EXEC returned a null array (*-1)</p>
            <p className="mt-1 text-xs text-muted">A WATCHed key was modified after WATCH, so none of the queued commands ran.</p>
          </div>
        ) : reply?.type === "array" ? (
          <ol className="divide-y divide-border overflow-hidden rounded-md border border-border">
            {reply.value.map((item, index) => (
              <li key={index} className="grid grid-cols-[1.5rem_minmax(0,1fr)_minmax(0,1fr)] items-start gap-3 px-3 py-2">
                <span className="font-mono text-xs text-faint">{index + 1}.</span>
                <CommandLine args={outcome.queued[index] ?? ["?"]} className="truncate font-mono text-xs" />
                <RespValueView value={item} className="text-xs" />
              </li>
            ))}
          </ol>
        ) : reply ? (
          <RespValueView value={reply} />
        ) : (
          <p className="font-mono text-xs text-err">{outcome.response.outcome === "failure" ? outcome.response.error : ""}</p>
        )}
      </CardBody>
    </Card>
  );
}
