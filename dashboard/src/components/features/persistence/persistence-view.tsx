"use client";

import { FileClock, HardDriveDownload } from "lucide-react";
import { useState, type ReactNode } from "react";

import { ResultCard } from "@/components/command/result-card";
import { TargetToggle } from "@/components/command/target-toggle";
import { Button } from "@/components/ui/button";
import { Card, CardBody, CardHeader } from "@/components/ui/card";
import { PageHeader } from "@/components/ui/page-header";
import { useCommandRunner } from "@/hooks/use-command";
import type { CommandResponse, Target } from "@/lib/resp/types";

function PersistenceAction({
  command,
  icon,
  title,
  summary,
  details,
}: {
  command: "SAVE" | "REWRITEAOF";
  icon: ReactNode;
  title: string;
  summary: string;
  details: string[];
}) {
  const { run, pending } = useCommandRunner("Persistence");
  const [target, setTarget] = useState<Target>("primary");
  const [response, setResponse] = useState<CommandResponse | null>(null);

  return (
    <div className="space-y-4">
      <Card>
        <CardHeader
          icon={icon}
          title={<span className="font-mono">{command}</span>}
          description={title}
          actions={<TargetToggle value={target} onChange={setTarget} />}
        />
        <CardBody className="space-y-3">
          <p className="text-sm text-text">{summary}</p>
          <ul className="space-y-1 text-xs text-muted">
            {details.map((line) => (
              <li key={line} className="flex gap-2">
                <span className="text-faint">–</span>
                {line}
              </li>
            ))}
          </ul>
          <Button variant="primary" loading={pending} onClick={async () => setResponse(await run([command], target))}>
            Run {command} on {target}
          </Button>
        </CardBody>
      </Card>
      <ResultCard response={response} title={`${command} response`} />
    </div>
  );
}

export function PersistenceView() {
  return (
    <>
      <PageHeader
        title="Persistence"
        description="Both mechanisms run inside the server process. The dashboard only sends the commands; it never touches files."
        commands={["SAVE", "REWRITEAOF"]}
      />
      <div className="grid gap-4 lg:grid-cols-2">
        <PersistenceAction
          command="SAVE"
          icon={<HardDriveDownload className="size-4" />}
          title="RDB snapshot"
          summary="Creates an RDB snapshot of the whole dataset."
          details={[
            "Custom binary format covering every value type plus absolute expiry timestamps.",
            "Written to a temporary file and renamed, so an interrupted save never destroys the last good snapshot.",
            "Loaded at startup when the AOF is empty.",
          ]}
        />
        <PersistenceAction
          command="REWRITEAOF"
          icon={<FileClock className="size-4" />}
          title="Append-only file rewrite"
          summary="Compacts the append-only log based on the current state."
          details={[
            "Every successful write is appended to the AOF as RESP.",
            "Rewrite reconstructs the logical dataset into a minimal new log (e.g. 100 RPUSHes → current list).",
            "A non-empty AOF is replayed at startup in preference to the RDB.",
          ]}
        />
      </div>
    </>
  );
}
