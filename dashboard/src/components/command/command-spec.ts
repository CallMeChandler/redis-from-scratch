export interface FieldSpec {
  name: string;
  label: string;
  placeholder?: string;
  defaultValue?: string;
  inputMode?: "text" | "numeric";
  /** Shared fields (e.g. "key") keep one value across every form on the page. */
  shared?: boolean;
  /** Allow running with this field empty (sends an empty bulk string). */
  optional?: boolean;
  grow?: number;
}

export interface CommandSpec {
  id: string;
  /** Command name shown on the card, e.g. "LRANGE". */
  title: string;
  description: string;
  fields: FieldSpec[];
  build: (values: Record<string, string>) => string[];
  /** Final rewrite applied at run time only (e.g. time-relative values), never during render. */
  prepare?: (args: string[]) => string[];
  kind?: "read" | "write";
}

/** Convenience for the common "COMMAND key" shape. */
export const keyField: FieldSpec = { name: "key", label: "Key", placeholder: "key", shared: true, grow: 2 };
