import type { Metadata } from "next";

import { SetsView } from "@/components/features/data-types/sets-view";

export const metadata: Metadata = { title: "Sets" };

export default function Page() {
  return <SetsView />;
}
