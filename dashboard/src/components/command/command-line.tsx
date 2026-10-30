import { displayArg } from "@/lib/resp/format";

/** A command rendered like a shell line: command name emphasized, args muted. */
export function CommandLine({ args, className }: { args: readonly string[]; className?: string }) {
  const [name, ...rest] = args;

  return (
    <code className={className ?? "font-mono text-[13px]"}>
      <span className="font-semibold text-text">{name?.toUpperCase()}</span>
      {rest.map((arg, index) => (
        <span key={index} className="text-muted">
          {" "}
          {displayArg(arg.length > 80 ? `${arg.slice(0, 77)}…` : arg)}
        </span>
      ))}
    </code>
  );
}
