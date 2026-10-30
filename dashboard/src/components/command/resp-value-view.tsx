import { cn } from "@/lib/cn";
import type { RespValue } from "@/lib/resp/types";

const TYPE_LABEL: Record<RespValue["type"], string> = {
  "simple-string": "simple",
  error: "error",
  integer: "integer",
  "bulk-string": "bulk",
  "null-bulk-string": "null bulk",
  array: "array",
  "null-array": "null array",
};

export function TypeTag({ type }: { type: RespValue["type"] }) {
  return (
    <span className="rounded bg-elevated px-1 py-px font-mono text-[10px] tracking-wide text-faint uppercase">
      {TYPE_LABEL[type]}
    </span>
  );
}

/** Typed, color-coded rendering of a RESP value tree. */
export function RespValueView({ value, className }: { value: RespValue; className?: string }) {
  return (
    <div className={cn("font-mono text-[13px] leading-relaxed", className)}>
      <Node value={value} />
    </div>
  );
}

function Node({ value }: { value: RespValue }) {
  switch (value.type) {
    case "simple-string":
      return <span className="text-t-simple">{value.value}</span>;
    case "error":
      return <span className="text-err">(error) {value.value}</span>;
    case "integer":
      return <span className="text-t-integer">(integer) {value.value}</span>;
    case "bulk-string":
      return value.value.includes("\n") ? (
        <pre className="whitespace-pre-wrap break-words text-t-string">{value.value.replace(/\r\n/g, "\n").trimEnd()}</pre>
      ) : (
        <span className="break-all text-t-string">&quot;{value.value}&quot;</span>
      );
    case "null-bulk-string":
    case "null-array":
      return <span className="text-t-null italic">(nil)</span>;
    case "array":
      if (value.value.length === 0) return <span className="text-t-null italic">(empty array)</span>;
      return (
        <ol className="space-y-0.5">
          {value.value.map((item, index) => (
            <li key={index} className="flex gap-2">
              <span className="shrink-0 text-right text-t-array tabular-nums select-none" style={{ minWidth: `${String(value.value.length).length + 1}ch` }}>
                {index + 1})
              </span>
              <div className="min-w-0 flex-1">
                <Node value={item} />
              </div>
            </li>
          ))}
        </ol>
      );
  }
}
