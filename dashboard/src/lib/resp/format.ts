import type { PlainValue, RespValue } from "./types";

/** Converts a typed RESP value into what a typical client library would return. */
export function toPlain(value: RespValue): PlainValue {
  switch (value.type) {
    case "simple-string":
    case "error":
    case "bulk-string":
      return value.value;
    case "integer":
      return value.value;
    case "null-bulk-string":
    case "null-array":
      return null;
    case "array":
      return value.value.map(toPlain);
  }
}

function quote(text: string): string {
  return `"${text
    .replace(/\\/g, "\\\\")
    .replace(/"/g, '\\"')
    .replace(/\n/g, "\\n")
    .replace(/\r/g, "\\r")
    .replace(/\t/g, "\\t")}"`;
}

/**
 * Formats a reply like redis-cli does in a TTY.
 * Multi-line bulk strings (e.g. INFO) are printed raw for readability.
 */
export function formatCli(value: RespValue, indent = ""): string {
  switch (value.type) {
    case "simple-string":
      return value.value;
    case "error":
      return `(error) ${value.value}`;
    case "integer":
      return `(integer) ${value.value}`;
    case "bulk-string":
      return value.value.includes("\n") ? value.value.replace(/\r\n/g, "\n").trimEnd() : quote(value.value);
    case "null-bulk-string":
      return "(nil)";
    case "null-array":
      return "(nil)";
    case "array": {
      if (value.value.length === 0) {
        return "(empty array)";
      }

      const width = String(value.value.length).length;

      return value.value
        .map((item, index) => {
          const label = `${String(index + 1).padStart(width, " ")}) `;
          const childIndent = indent + " ".repeat(label.length);
          const body = formatCli(item, childIndent);
          return `${index === 0 ? "" : indent}${label}${body}`;
        })
        .join("\n");
    }
  }
}

/** One-line summary used by the event log and history lists. */
export function summarize(value: RespValue, max = 48): string {
  let text: string;

  switch (value.type) {
    case "simple-string":
      text = value.value;
      break;
    case "error":
      text = value.value;
      break;
    case "integer":
      text = `(integer) ${value.value}`;
      break;
    case "bulk-string":
      text = value.value.includes("\n") ? `${value.value.split("\n")[0].trim()} …` : quote(value.value);
      break;
    case "null-bulk-string":
    case "null-array":
      text = "(nil)";
      break;
    case "array":
      text =
        value.value.length === 0
          ? "(empty array)"
          : `[${value.value.map((item) => summarize(item, 16)).join(", ")}]`;
      break;
  }

  return text.length > max ? `${text.slice(0, max - 1)}…` : text;
}

/** Makes CR/LF visible so RESP framing can be read on screen. */
export function visualizeResp(raw: string): string {
  return raw.replace(/\r\n/g, "\\r\\n\n").replace(/\n$/, "");
}

/** Quotes a single argument for display only if it needs it. */
export function displayArg(arg: string): string {
  return arg === "" || /[\s"'\\]/.test(arg) ? quote(arg) : arg;
}

export function displayCommand(args: readonly string[]): string {
  return args.map(displayArg).join(" ");
}

export function formatBytes(bytes: number): string {
  if (!Number.isFinite(bytes)) {
    return "—";
  }

  const units = ["B", "KB", "MB", "GB", "TB"];
  let value = bytes;
  let unit = 0;

  while (Math.abs(value) >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }

  return unit === 0 ? `${value} B` : `${value.toFixed(value >= 100 ? 0 : 1)} ${units[unit]}`;
}

export function formatDuration(totalSeconds: number): string {
  if (!Number.isFinite(totalSeconds) || totalSeconds < 0) {
    return "—";
  }

  const days = Math.floor(totalSeconds / 86400);
  const hours = Math.floor((totalSeconds % 86400) / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = Math.floor(totalSeconds % 60);

  if (days > 0) return `${days}d ${hours}h`;
  if (hours > 0) return `${hours}h ${minutes}m`;
  if (minutes > 0) return `${minutes}m ${seconds}s`;
  return `${seconds}s`;
}

export function formatLatency(ms: number): string {
  if (ms < 1) return `${ms.toFixed(2)} ms`;
  if (ms < 100) return `${ms.toFixed(1)} ms`;
  return `${Math.round(ms)} ms`;
}
