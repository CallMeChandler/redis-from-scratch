import type { Metadata } from "next";

import { ReplicationView } from "@/components/features/replication/replication-view";

export const metadata: Metadata = { title: "Replication" };

export default function Page() {
  return <ReplicationView />;
}
