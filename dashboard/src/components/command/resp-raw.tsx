import { cn } from "@/lib/cn";

/**
 * Shows raw RESP bytes with CRLF made visible, one protocol line per row,
 * so framing (`*3`, `$5`, payload) can be read directly.
 */
export function RespRaw({ raw, className, maxHeight = "16rem" }: { raw: string; className?: string; maxHeight?: string }) {
  const lines = raw.split("\r\n");
  if (lines[lines.length - 1] === "") lines.pop();

  return (
    <pre
      className={cn("overflow-auto rounded-md border border-border bg-bg p-2.5 font-mono text-xs leading-5", className)}
      style={{ maxHeight }}
    >
      {lines.length === 0 ? (
        <span className="text-faint">(empty)</span>
      ) : (
        lines.map((line, index) => (
          <div key={index} className="whitespace-pre-wrap break-all">
            <span className={prefixClass(line)}>{line}</span>
            <span className="text-faint select-none">\r\n</span>
          </div>
        ))
      )}
    </pre>
  );
}

function prefixClass(line: string): string {
  switch (line[0]) {
    case "*":
      return "text-t-array";
    case "$":
      return "text-faint";
    case "+":
      return "text-t-simple";
    case "-":
      return "text-err";
    case ":":
      return "text-t-integer";
    default:
      return "text-text";
  }
}
