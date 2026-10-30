import type { Metadata } from "next";

import { HashesView } from "@/components/features/data-types/hashes-view";

export const metadata: Metadata = { title: "Hashes" };

export default function Page() {
  return <HashesView />;
}
