import type { Metadata } from "next";

import { PersistenceView } from "@/components/features/persistence/persistence-view";

export const metadata: Metadata = { title: "Persistence" };

export default function Page() {
  return <PersistenceView />;
}
