import type { Metadata } from "next";

import { MemoryView } from "@/components/features/memory/memory-view";

export const metadata: Metadata = { title: "Memory / LRU" };

export default function Page() {
  return <MemoryView />;
}
