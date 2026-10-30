import type { Metadata } from "next";

import { OverviewView } from "@/components/features/overview/overview-view";

export const metadata: Metadata = { title: { absolute: "Redis From Scratch" } };

export default function Page() {
  return <OverviewView />;
}
