import type { Metadata } from "next";

import { TtlView } from "@/components/features/ttl/ttl-view";

export const metadata: Metadata = { title: "TTL" };

export default function Page() {
  return <TtlView />;
}
