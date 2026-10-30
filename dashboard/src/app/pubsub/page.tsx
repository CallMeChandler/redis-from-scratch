import type { Metadata } from "next";

import { PubSubView } from "@/components/features/pubsub/pubsub-view";

export const metadata: Metadata = { title: "Pub/Sub" };

export default function Page() {
  return <PubSubView />;
}
