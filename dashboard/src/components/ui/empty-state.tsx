import type { ReactNode } from "react";

import { cn } from "@/lib/cn";

export function EmptyState({
  icon,
  title,
  description,
  className,
}: {
  icon?: ReactNode;
  title: string;
  description?: ReactNode;
  className?: string;
}) {
  return (
    <div className={cn("flex flex-col items-center justify-center gap-1.5 px-6 py-8 text-center", className)}>
      {icon && <div className="mb-1 text-faint">{icon}</div>}
      <p className="text-sm font-medium text-muted">{title}</p>
      {description && <p className="max-w-sm text-xs text-faint">{description}</p>}
    </div>
  );
}
