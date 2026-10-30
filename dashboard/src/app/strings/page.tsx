import type { Metadata } from "next";

import { StringsView } from "@/components/features/data-types/strings-view";

export const metadata: Metadata = { title: "Strings" };

export default function Page() {
  return <StringsView />;
}
