import type { ReactNode } from "react";

export function PageHeader({
  title,
  description,
  commands,
  actions,
}: {
  title: string;
  description: ReactNode;
  /** Commands this page exercises, shown as chips. */
  commands?: readonly string[];
  actions?: ReactNode;
}) {
  return (
    <div className="mb-5 flex flex-wrap items-end justify-between gap-4">
      <div className="min-w-0">
        <h1 className="text-xl font-semibold tracking-tight text-text">{title}</h1>
        <p className="mt-1 max-w-2xl text-sm text-muted">{description}</p>
        {commands && commands.length > 0 && (
          <div className="mt-2.5 flex flex-wrap gap-1">
            {commands.map((command) => (
              <code
                key={command}
                className="rounded border border-border bg-panel-2 px-1.5 py-px font-mono text-[11px] text-muted"
              >
                {command}
              </code>
            ))}
          </div>
        )}
      </div>
      {actions && <div className="flex items-center gap-2">{actions}</div>}
    </div>
  );
}
