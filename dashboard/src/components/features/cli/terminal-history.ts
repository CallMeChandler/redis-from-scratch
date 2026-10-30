const KEY = "rfs-cli-history";
const MAX = 200;

/** Per-browser command history; storage may be unavailable, so every access is guarded. */
export function loadHistory(): string[] {
  if (typeof window === "undefined") return [];
  try {
    const raw = localStorage.getItem(KEY);
    const parsed: unknown = raw ? JSON.parse(raw) : [];
    return Array.isArray(parsed) ? parsed.filter((item): item is string => typeof item === "string").slice(-MAX) : [];
  } catch {
    return [];
  }
}

export function saveHistory(history: string[]): void {
  try {
    localStorage.setItem(KEY, JSON.stringify(history.slice(-MAX)));
  } catch {
    // ignore: history is a convenience
  }
}
