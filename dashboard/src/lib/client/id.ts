/** crypto.randomUUID needs a secure context; fall back for plain-HTTP LAN access. */
export function makeId(): string {
  if (typeof crypto !== "undefined" && typeof crypto.randomUUID === "function" && globalThis.isSecureContext) {
    return crypto.randomUUID();
  }
  return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 10)}`;
}
