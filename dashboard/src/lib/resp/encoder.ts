import { Buffer } from "node:buffer";

const CRLF = "\r\n";

/**
 * Encodes a command the way real clients do: an array of bulk strings.
 * Lengths are UTF-8 byte lengths, not JavaScript string lengths.
 */
export function encodeCommand(args: readonly string[]): Buffer {
  const parts: Buffer[] = [Buffer.from(`*${args.length}${CRLF}`)];

  for (const arg of args) {
    const payload = Buffer.from(arg, "utf8");
    parts.push(Buffer.from(`$${payload.length}${CRLF}`), payload, Buffer.from(CRLF));
  }

  return Buffer.concat(parts);
}

/** Encodes several commands into one buffer, i.e. a pipelined write. */
export function encodePipeline(commands: readonly (readonly string[])[]): Buffer {
  return Buffer.concat(commands.map(encodeCommand));
}
