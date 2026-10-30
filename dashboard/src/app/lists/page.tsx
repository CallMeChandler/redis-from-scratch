import type { Metadata } from "next";

import { ListsView } from "@/components/features/data-types/lists-view";

export const metadata: Metadata = { title: "Lists" };

export default function Page() {
  return <ListsView />;
}
