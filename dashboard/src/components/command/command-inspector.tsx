import type { ReactNode } from "react";

import { CopyButton } from "@/components/ui/copy-button";
import { displayCommand, formatLatency } from "@/lib/resp/format";
import type { CommandResponse } from "@/lib/resp/types";

import { CommandLine } from "./command-line";
import { OutcomeBadge } from "./outcome-badge";
import { RespRaw } from "./resp-raw";
import { RespValueView, TypeTag } from "./resp-value-view";

function Section({ label, copy, children }: { label: string; copy?: string; children: ReactNode }) {
  return (
    <div className="space-y-1.5">
      <div className="flex items-center justify-between">
        <h3 className="text-[11px] font-medium tracking-wide text-faint uppercase">{label}</h3>
        {copy !== undefined && <CopyButton text={copy} label={`Copy ${label.toLowerCase()}`} />}
      </div>
      {children}
    </div>
  );
}

/**
 * Wire-level view of one command: what was sent, what came back,
 * how it parsed, and how long the round trip took.
 */
export function CommandInspector({ response }: { response: CommandResponse }) {
  const time = new Date(response.startedAt).toLocaleTimeString([], { hour12: false });

  return (
    <div className="space-y-4">
      <div className="flex flex-wrap items-center gap-2 text-xs text-muted">
        <OutcomeBadge response={response} />
        <span className="font-mono uppercase">{response.target}</span>
        <span className="text-faint">·</span>
        <span>{response.via === "http" ? "shared socket (HTTP)" : "dedicated session socket"}</span>
        <span className="text-faint">·</span>
        <span className="font-mono">{time}</span>
        {response.outcome !== "failure" && (
          <>
            <span className="text-faint">·</span>
            <span className="font-mono text-text">{formatLatency(response.latencyMs)}</span>
          </>
        )}
      </div>

      <Section label="Command" copy={displayCommand(response.command)}>
        <div className="rounded-md border border-border bg-bg px-2.5 py-2">
          <CommandLine args={response.command} />
        </div>
      </Section>

      {response.rawRequest !== undefined && (
        <Section label={`RESP sent · ${byteLength(response.rawRequest)} bytes`} copy={response.rawRequest}>
          <RespRaw raw={response.rawRequest} />
        </Section>
      )}

      {response.outcome === "failure" ? (
        <Section label="Failure">
          <p className="rounded-md border border-err/30 bg-err-soft px-2.5 py-2 font-mono text-xs text-err">{response.error}</p>
        </Section>
      ) : (
        <>
          <Section label={`RESP received · ${byteLength(response.rawResponse)} bytes`} copy={response.rawResponse}>
            <RespRaw raw={response.rawResponse} />
          </Section>
          <Section label={response.replies.length > 1 ? `Parsed · ${response.replies.length} frames` : "Parsed"}>
            <div className="space-y-2">
              {response.replies.map((reply, index) => (
                <div key={index} className="rounded-md border border-border bg-bg px-2.5 py-2">
                  <div className="mb-1">
                    <TypeTag type={reply.type} />
                  </div>
                  <RespValueView value={reply} />
                </div>
              ))}
            </div>
          </Section>
        </>
      )}
    </div>
  );
}

function byteLength(text: string): number {
  return new TextEncoder().encode(text).length;
}
