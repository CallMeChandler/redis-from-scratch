import type { InfoSections } from "./resp/types";

/** Parses INFO output (`# Section` headers followed by `key:value` lines). */
export function parseInfo(raw: string): InfoSections {
  const sections: InfoSections = {};
  let current = "default";

  for (const line of raw.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (trimmed === "") continue;

    if (trimmed.startsWith("#")) {
      current = trimmed.slice(1).trim().toLowerCase();
      sections[current] ??= {};
      continue;
    }

    const separator = trimmed.indexOf(":");
    if (separator === -1) continue;

    sections[current] ??= {};
    sections[current][trimmed.slice(0, separator)] = trimmed.slice(separator + 1);
  }

  return sections;
}

/** Flattens all sections into one lookup, e.g. info.used_memory. */
export function flattenInfo(sections: InfoSections): Record<string, string> {
  return Object.assign({}, ...Object.values(sections));
}

export interface KeyspaceStats {
  keys: number;
  expires: number;
}

/** Parses `db0:keys=3,expires=1`. */
export function parseKeyspace(value: string | undefined): KeyspaceStats | null {
  if (!value) return null;

  const fields = Object.fromEntries(
    value.split(",").map((pair) => {
      const [key, raw] = pair.split("=");
      return [key, Number(raw)];
    }),
  );

  return { keys: fields.keys ?? 0, expires: fields.expires ?? 0 };
}

export function numberField(info: Record<string, string>, key: string): number | null {
  const value = info[key];
  if (value === undefined) return null;
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
}

export const INFO_SECTIONS = ["server", "clients", "memory", "stats", "replication", "keyspace"] as const;
export type InfoSection = (typeof INFO_SECTIONS)[number];
