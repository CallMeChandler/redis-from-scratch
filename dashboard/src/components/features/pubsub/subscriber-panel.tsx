"use client";

import { BellOff, BellRing, Code2, Radio, Trash2, X } from "lucide-react";
import { useState, type FormEvent } from "react";

import { RespRaw } from "@/components/command/resp-raw";
import { SessionStatusBar } from "@/components/command/session-status";
import { useToast } from "@/components/providers/toast-provider";
import { Button } from "@/components/ui/button";
import { Card, CardHeader } from "@/components/ui/card";
import { EmptyState } from "@/components/ui/empty-state";
import { Input } from "@/components/ui/input";
import { useSession } from "@/hooks/use-session";
import type { PushEvent } from "@/lib/bridge/ws-protocol";
import { cn } from "@/lib/cn";

const MAX_MESSAGES = 200;

/**
 * One subscriber = one dedicated TCP connection in subscribe mode on the
 * bridge. Messages are real `*3 message <channel> <payload>` pushes from
 * the C++ server, relayed over the WebSocket as they arrive.
 */
export function SubscriberPanel({ index, onRemove }: { index: number; onRemove?: () => void }) {
  const notify = useToast();
  const [messages, setMessages] = useState<PushEvent[]>([]);
  const [channel, setChannel] = useState("news");
  const [showRaw, setShowRaw] = useState(false);

  const session = useSession({
    target: "primary",
    kind: "subscriber",
    origin: `Pub/Sub · sub ${index}`,
    onPush: (push) => setMessages((items) => [push, ...items].slice(0, MAX_MESSAGES)),
  });

  const subscriptions = session.state.subscriptions;
  const open = session.status === "open";

  const exec = async (args: string[]) => {
    const response = await session.execute(args);
    if (response.outcome === "redis-error") notify({ tone: "error", title: args[0], description: response.redisError });
  };

  const subscribe = (event: FormEvent) => {
    event.preventDefault();
    const channels = channel.trim().split(/\s+/).filter(Boolean);
    if (channels.length) void exec(["SUBSCRIBE", ...channels]);
  };

  return (
    <Card className="flex min-w-0 flex-col">
      <CardHeader
        icon={<Radio className="size-4" />}
        title={`Subscriber ${index}`}
        description={subscriptions.length ? `${subscriptions.length} channel(s) · subscribe mode` : "not subscribed"}
        actions={
          <>
            <button
              type="button"
              onClick={() => setShowRaw(!showRaw)}
              className={cn("rounded p-1 hover:bg-hover", showRaw ? "text-accent" : "text-faint hover:text-text")}
              aria-pressed={showRaw}
              title="Show raw RESP frames"
            >
              <Code2 className="size-3.5" />
            </button>
            <button type="button" onClick={() => setMessages([])} className="rounded p-1 text-faint hover:bg-hover hover:text-text" title="Clear messages">
              <Trash2 className="size-3.5" />
            </button>
            {onRemove && (
              <button type="button" onClick={onRemove} className="rounded p-1 text-faint hover:bg-hover hover:text-text" title="Remove subscriber (closes its socket)">
                <X className="size-3.5" />
              </button>
            )}
          </>
        }
      />
      <div className="space-y-3 border-b border-border p-4">
        <SessionStatusBar status={session.status} address={session.address} error={session.error} onReconnect={session.reopen} label="Subscriber socket" />
        <form onSubmit={subscribe} className="flex gap-2">
          <Input value={channel} onChange={(event) => setChannel(event.target.value)} placeholder="channel (space separated)" disabled={!open} />
          <Button type="submit" variant="primary" icon={<BellRing className="size-3.5" />} disabled={!open || !channel.trim()}>
            SUBSCRIBE
          </Button>
          <Button icon={<BellOff className="size-3.5" />} disabled={!open || subscriptions.length === 0} onClick={() => exec(["UNSUBSCRIBE"])} title="UNSUBSCRIBE from all channels">
            All
          </Button>
        </form>
        <div className="flex min-h-6 flex-wrap gap-1.5">
          {subscriptions.length === 0 ? (
            <span className="font-mono text-[11px] text-faint">No active subscriptions.</span>
          ) : (
            subscriptions.map((name) => (
              <span key={name} className="inline-flex items-center gap-1 rounded-full border border-ok/30 bg-ok-soft py-0.5 pr-1 pl-2.5 font-mono text-xs text-ok">
                {name}
                <button type="button" onClick={() => exec(["UNSUBSCRIBE", name])} className="rounded-full p-0.5 hover:bg-ok/20" aria-label={`Unsubscribe from ${name}`}>
                  <X className="size-3" />
                </button>
              </span>
            ))
          )}
        </div>
      </div>

      <div className="min-h-64 flex-1">
        <div className="flex items-center justify-between px-4 py-2 text-[11px] font-medium tracking-wide text-faint uppercase">
          <span>Incoming messages</span>
          <span className="font-mono">{messages.length}</span>
        </div>
        {messages.length === 0 ? (
          <EmptyState title="Waiting for messages" description="Subscribe to a channel, then PUBLISH to it from the publisher panel or redis-cli." className="py-8" />
        ) : (
          <ul className="max-h-[26rem] space-y-1.5 overflow-auto px-4 pb-4">
            {messages.map((push) => (
              <li key={push.id} className="animate-slide-in rounded-md border border-border bg-bg px-3 py-2">
                <div className="flex items-center gap-2 font-mono text-[11px]">
                  <span className="text-faint">{new Date(push.receivedAt).toLocaleTimeString([], { hour12: false })}</span>
                  <span className="rounded bg-accent-soft px-1.5 text-accent">{push.message?.channel ?? "push"}</span>
                </div>
                <p className="mt-1 font-mono text-[13px] break-all text-t-string">{push.message ? push.message.payload : push.raw}</p>
                {showRaw && <RespRaw raw={push.raw} className="mt-2" />}
              </li>
            ))}
          </ul>
        )}
      </div>
    </Card>
  );
}
