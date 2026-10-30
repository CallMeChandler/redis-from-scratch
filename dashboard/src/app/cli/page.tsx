import type { Metadata } from "next";

import { CliView } from "@/components/features/cli/cli-view";

export const metadata: Metadata = { title: "Raw CLI" };

export default function Page() {
  return <CliView />;
}
