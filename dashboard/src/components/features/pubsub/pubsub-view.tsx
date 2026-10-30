"use client";

import { Plus } from "lucide-react";
import { useState } from "react";

import { Button } from "@/components/ui/button";
import { PageHeader } from "@/components/ui/page-header";

import { PublisherPanel } from "./publisher-panel";
import { SubscriberPanel } from "./subscriber-panel";

const MAX_SUBSCRIBERS = 4;

export function PubSubView() {
  const [subscribers, setSubscribers] = useState<number[]>([1]);
  const nextId = Math.max(0, ...subscribers) + 1;

  return (
    <>
      <PageHeader
        title="Pub/Sub"
        description="Subscribers hold long-lived TCP connections on the Node bridge. Messages are pushed from the C++ server and streamed to this page over a WebSocket."
        commands={["SUBSCRIBE", "UNSUBSCRIBE", "PUBLISH"]}
        actions={
          <Button
            icon={<Plus className="size-3.5" />}
            disabled={subscribers.length >= MAX_SUBSCRIBERS}
            onClick={() => setSubscribers([...subscribers, nextId])}
          >
            Add subscriber
          </Button>
        }
      />
      <div className="grid gap-4 xl:grid-cols-[minmax(0,1fr)_22rem]">
        <div className={subscribers.length > 1 ? "grid min-w-0 gap-4 lg:grid-cols-2" : "min-w-0"}>
          {subscribers.map((id) => (
            <SubscriberPanel
              key={id}
              index={id}
              onRemove={subscribers.length > 1 ? () => setSubscribers(subscribers.filter((item) => item !== id)) : undefined}
            />
          ))}
        </div>
        <PublisherPanel />
      </div>
    </>
  );
}
