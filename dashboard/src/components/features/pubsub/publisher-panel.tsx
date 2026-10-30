"use client";

import { Megaphone, Send, Zap } from "lucide-react";
import { useState, type FormEvent } from "react";

import { ResultCard } from "@/components/command/result-card";
import { Button } from "@/components/ui/button";
import { Card, CardBody, CardHeader } from "@/components/ui/card";
import { Field, Input } from "@/components/ui/input";
import { useCommandRunner } from "@/hooks/use-command";
import type { CommandResponse, CommandRoundTrip } from "@/lib/resp/types";

function receivers(response: CommandRoundTrip) {
  const reply = response.replies[0];
  if (reply.type !== "integer") return undefined;

  return (
    <div className="flex items-baseline gap-3 rounded-md border border-border bg-bg px-3 py-3">
      <span className="font-mono text-2xl font-semibold text-t-integer tabular-nums">{reply.value}</span>
      <span className="text-sm text-muted">
        subscriber{reply.value === 1 ? "" : "s"} received the message on <code className="font-mono text-text">{response.command[1]}</code>
      </span>
    </div>
  );
}

/** PUBLISH runs over the shared stateless socket, separate from every subscriber. */
export function PublisherPanel() {
  const { run, pending } = useCommandRunner("Pub/Sub · publisher");
  const [channel, setChannel] = useState("news");
  const [message, setMessage] = useState("hello from the dashboard");
  const [last, setLast] = useState<CommandResponse | null>(null);

  const publish = async (event?: FormEvent) => {
    event?.preventDefault();
    setLast(await run(["PUBLISH", channel, message]));
  };

  const burst = async () => {
    for (let index = 1; index <= 10; index += 1) {
      const response = await run(["PUBLISH", channel, `${message} #${index}`], "primary", { auto: index < 10 });
      if (index === 10) setLast(response);
    }
  };

  return (
    <div className="space-y-4">
      <Card>
        <CardHeader icon={<Megaphone className="size-4" />} title="Publisher" description="PUBLISH over the shared socket; returns how many subscribers got it" />
        <CardBody>
          <form onSubmit={publish} className="space-y-3">
            <Field label="Channel">
              <Input value={channel} onChange={(event) => setChannel(event.target.value)} />
            </Field>
            <Field label="Message">
              <Input value={message} onChange={(event) => setMessage(event.target.value)} />
            </Field>
            <div className="flex gap-2">
              <Button type="submit" variant="primary" icon={<Send className="size-3.5" />} loading={pending} disabled={!channel}>
                PUBLISH
              </Button>
              <Button icon={<Zap className="size-3.5" />} onClick={burst} disabled={!channel} title="Publish 10 numbered messages back to back">
                Burst ×10
              </Button>
            </div>
          </form>
        </CardBody>
      </Card>
      <ResultCard response={last} title="Last publish" visualize={receivers} emptyText="PUBLISH returns the number of clients that received the message." />
    </div>
  );
}
