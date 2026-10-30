"use client";

import { Eraser } from "lucide-react";
import { useEffect, useRef, useState, type ClipboardEvent, type KeyboardEvent } from "react";

import { TargetToggle } from "@/components/command/target-toggle";
import { useActivity } from "@/components/providers/activity-provider";
import { Badge } from "@/components/ui/badge";
import { Kbd } from "@/components/ui/kbd";
import { useSession } from "@/hooks/use-session";
import type { PushEvent } from "@/lib/bridge/ws-protocol";
import { cn } from "@/lib/cn";
import { COMMANDS, completeCommand, hintFor } from "@/lib/commands";
import { formatCli, formatLatency } from "@/lib/resp/format";
import { TokenizeError, tokenize } from "@/lib/resp/tokenize";
import type { CommandResponse, Target } from "@/lib/resp/types";

import { loadHistory, saveHistory } from "./terminal-history";

type LineBody =
  | { kind: "input"; text: string; target: Target; tag: string }
  | { kind: "reply"; response: CommandResponse }
  | { kind: "push"; push: PushEvent }
  | { kind: "note"; text: string; tone?: "muted" | "warn" | "err" };

type Line = LineBody & { id: number };

const MAX_LINES = 600;
const PROMPT = "redis-from-scratch";

const HELP = [
  "Commands supported by this server (arity as implemented):",
  ...COMMANDS.map((command) => `  ${command.name.padEnd(12)} ${command.args.padEnd(30)} ${command.summary}`),
  "",
  "Built-ins: help, clear (Ctrl+L). History: ↑/↓. Tab completes command names.",
  'Quoting: SET msg "hello world"   SET k \'raw text\'   SET k "line\\nbreak"',
].join("\n");

function replyText(response: CommandResponse): string {
  if (response.outcome === "failure") return `(bridge error) ${response.error}`;
  return response.replies.map((reply) => formatCli(reply)).join("\n");
}

export function Terminal() {
  const [target, setTarget] = useState<Target>("primary");
  const [lines, setLines] = useState<Line[]>([
    { id: 0, kind: "note", text: "Connected through a dedicated TCP session. MULTI, WATCH and SUBSCRIBE work like in redis-cli. Type help." },
  ]);
  const [input, setInput] = useState("");
  const [history, setHistory] = useState<string[]>(loadHistory);
  const [cursor, setCursor] = useState<number | null>(null);
  const nextId = useRef(1);
  const inputRef = useRef<HTMLInputElement>(null);
  const scrollRef = useRef<HTMLDivElement>(null);
  const { inspect } = useActivity();

  const append = (...items: LineBody[]) =>
    setLines((current) => [...current, ...items.map((item): Line => ({ ...item, id: nextId.current++ }))].slice(-MAX_LINES));

  const session = useSession({
    target,
    kind: "cli",
    origin: "CLI",
    onPush: (push) => append({ kind: "push", push }),
  });

  useEffect(() => {
    scrollRef.current?.scrollTo({ top: scrollRef.current.scrollHeight });
  }, [lines]);

  const { inMulti, subscriptions } = session.state;
  const tag = subscriptions.length > 0 ? "(subscribed)" : inMulti ? "(TX)" : "";

  const runLine = async (text: string) => {
    const trimmed = text.trim();
    if (!trimmed) return;

    setHistory((current) => {
      const next = [...current.filter((item) => item !== trimmed), trimmed];
      saveHistory(next);
      return next;
    });
    setCursor(null);

    if (trimmed === "clear") {
      setLines([]);
      return;
    }

    append({ kind: "input", text: trimmed, target, tag });

    if (trimmed === "help" || trimmed === "?") {
      append({ kind: "note", text: HELP, tone: "muted" });
      return;
    }

    let args: string[];
    try {
      args = tokenize(trimmed);
    } catch (error) {
      append({ kind: "note", text: `Invalid argument(s): ${error instanceof TokenizeError ? error.message : String(error)}`, tone: "err" });
      return;
    }

    const response = await session.execute(args);
    append({ kind: "reply", response });
  };

  const onKeyDown = (event: KeyboardEvent<HTMLInputElement>) => {
    if (event.key === "Enter") {
      event.preventDefault();
      const value = input;
      setInput("");
      void runLine(value);
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      if (history.length === 0) return;
      const index = cursor === null ? history.length - 1 : Math.max(0, cursor - 1);
      setCursor(index);
      setInput(history[index]);
    } else if (event.key === "ArrowDown") {
      event.preventDefault();
      if (cursor === null) return;
      const index = cursor + 1;
      if (index >= history.length) {
        setCursor(null);
        setInput("");
      } else {
        setCursor(index);
        setInput(history[index]);
      }
    } else if (event.key === "Tab") {
      const [first, ...rest] = input.split(" ");
      if (rest.length === 0 && first) {
        event.preventDefault();
        const matches = completeCommand(first);
        if (matches.length === 1) setInput(`${matches[0]} `);
        else if (matches.length > 1) append({ kind: "note", text: matches.join("  "), tone: "muted" });
      }
    } else if (event.key === "l" && event.ctrlKey) {
      event.preventDefault();
      setLines([]);
    }
  };

  /** Pasting several lines runs them one after another. */
  const onPaste = async (event: ClipboardEvent<HTMLInputElement>) => {
    const text = event.clipboardData.getData("text");
    if (!text.includes("\n")) return;
    event.preventDefault();
    for (const line of text.split(/\r?\n/)) await runLine(line);
  };

  // redis-cli style ghost hint: the argument being typed counts as typed.
  let hint = "";
  try {
    const tokens = tokenize(input);
    if (tokens.length > 0) hint = hintFor(tokens[0], tokens.length - 1);
  } catch {
    hint = "";
  }

  const open = session.status === "open";

  return (
    <div className="flex h-[calc(100dvh-13rem)] min-h-[28rem] flex-col overflow-hidden rounded-xl border border-border bg-[#07090b]">
      <div className="flex items-center gap-3 border-b border-white/10 bg-white/[0.03] px-3 py-2">
        <div className="flex gap-1.5" aria-hidden>
          <span className="size-2.5 rounded-full bg-[#ff5f57]/80" />
          <span className="size-2.5 rounded-full bg-[#febc2e]/80" />
          <span className="size-2.5 rounded-full bg-[#28c840]/80" />
        </div>
        <span className="font-mono text-xs text-[#8b96a3]">
          {PROMPT} · {session.address || "connecting…"}
        </span>
        <Badge tone={open ? "ok" : session.status === "connecting" ? "warn" : "err"}>{open ? "session open" : session.status}</Badge>
        {inMulti && <Badge tone="warn">multi</Badge>}
        {subscriptions.length > 0 && <Badge tone="accent">subscribed: {subscriptions.join(", ")}</Badge>}
        <div className="ml-auto flex items-center gap-2">
          <TargetToggle
            value={target}
            onChange={(next) => {
              if (next === target) return;
              setTarget(next);
              append({ kind: "note", text: `Switched to ${next}: opened a new session (connection state such as MULTI or subscriptions does not carry over).`, tone: "warn" });
            }}
          />
          <button type="button" onClick={() => setLines([])} className="rounded p-1 text-[#5a6572] hover:bg-white/5 hover:text-[#e5ebf1]" title="Clear (Ctrl+L)">
            <Eraser className="size-3.5" />
          </button>
        </div>
      </div>

      <div ref={scrollRef} className="flex-1 overflow-y-auto px-4 py-3 font-mono text-[13px] leading-[1.6] text-[#e5ebf1]" onClick={() => window.getSelection()?.isCollapsed && inputRef.current?.focus()}>
        {lines.map((line) => (
          <TerminalLine key={line.id} line={line} onInspect={inspect} />
        ))}
        {!open && session.status !== "connecting" && (
          <p className="text-[#ff6b63]">
            (session {session.status}{session.error ? `: ${session.error}` : ""}){" "}
            <button type="button" className="underline underline-offset-2" onClick={session.reopen}>
              reconnect
            </button>
          </p>
        )}

        <div className="flex items-center">
          <Prompt target={target} tag={tag} />
          <div className="relative flex-1">
            <div className="pointer-events-none absolute inset-0 overflow-hidden whitespace-pre" aria-hidden>
              <span className="invisible">{input}</span>
              <span className="text-[#5a6572]">{input && hint ? (input.endsWith(" ") ? hint : ` ${hint}`) : ""}</span>
            </div>
            <input
              ref={inputRef}
              autoFocus
              value={input}
              onChange={(event) => {
                setInput(event.target.value);
                setCursor(null);
              }}
              onKeyDown={onKeyDown}
              onPaste={onPaste}
              spellCheck={false}
              autoComplete="off"
              aria-label="Command"
              className="relative w-full bg-transparent text-[#e5ebf1] caret-[#5aa2ff] outline-none focus-visible:outline-none"
            />
          </div>
        </div>
      </div>

      <div className="flex items-center gap-3 border-t border-white/10 px-3 py-1.5 font-mono text-[10.5px] text-[#5a6572]">
        <span>
          <Kbd>↑</Kbd> <Kbd>↓</Kbd> history
        </span>
        <span>
          <Kbd>Tab</Kbd> complete
        </span>
        <span>
          <Kbd>Ctrl</Kbd>+<Kbd>L</Kbd> clear
        </span>
        <span className="ml-auto">click a reply to inspect its RESP</span>
      </div>
    </div>
  );
}

function Prompt({ target, tag }: { target: Target; tag: string }) {
  return (
    <span className="shrink-0 pr-2 select-none">
      <span className={target === "primary" ? "text-[#5aa2ff]" : "text-[#b48cff]"}>{PROMPT}</span>
      {target === "replica" && <span className="text-[#b48cff]">(replica)</span>}
      {tag && <span className="text-[#f0b544]">{tag}</span>}
      <span className="text-[#8a96a3]">&gt;</span>
    </span>
  );
}

function TerminalLine({ line, onInspect }: { line: Line; onInspect: (response: CommandResponse) => void }) {
  switch (line.kind) {
    case "input":
      return (
        <div className="mt-1 flex">
          <Prompt target={line.target} tag={line.tag} />
          <span className="break-all whitespace-pre-wrap">{line.text}</span>
        </div>
      );
    case "reply": {
      const { response } = line;
      const error = response.outcome !== "reply";
      return (
        <button type="button" onClick={() => onInspect(response)} className="group block w-full rounded text-left hover:bg-white/[0.03]">
          <span className="flex items-start gap-4">
            <span className={cn("min-w-0 flex-1 break-all whitespace-pre-wrap", error ? "text-[#ff6b63]" : "text-[#e5ebf1]")}>{replyText(response)}</span>
            {response.outcome !== "failure" && (
              <span className="shrink-0 text-[11px] text-[#5a6572] group-hover:text-[#8a96a3]">{formatLatency(response.latencyMs)}</span>
            )}
          </span>
        </button>
      );
    }
    case "push":
      return <div className="break-all whitespace-pre-wrap text-[#7cb5ff]">{formatCli(line.push.value)}</div>;
    case "note":
      return (
        <div
          className={cn(
            "break-words whitespace-pre-wrap",
            line.tone === "err" ? "text-[#ff6b63]" : line.tone === "warn" ? "text-[#f0b544]" : "text-[#8a96a3]",
          )}
        >
          {line.text}
        </div>
      );
  }
}
