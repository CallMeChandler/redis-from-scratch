import type { Metadata } from "next";

import { SortedSetsView } from "@/components/features/data-types/sorted-sets-view";

export const metadata: Metadata = { title: "Sorted Sets" };

export default function Page() {
  return <SortedSetsView />;
}
