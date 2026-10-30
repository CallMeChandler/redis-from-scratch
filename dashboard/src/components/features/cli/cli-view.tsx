import { PageHeader } from "@/components/ui/page-header";

import { Terminal } from "./terminal";

export function CliView() {
  return (
    <>
      <PageHeader
        title="Raw CLI"
        description="A terminal on a dedicated session socket. Input is tokenized like redis-cli, encoded as a RESP array of bulk strings, and replies are formatted from the parsed frames."
      />
      <Terminal />
    </>
  );
}
