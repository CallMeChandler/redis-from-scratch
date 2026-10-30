import { Buffer } from "node:buffer";

import type { RespValue } from "./types";

export class RespProtocolError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "RespProtocolError";
  }
}

export interface ParsedFrame {
  value: RespValue;
  /** Exact bytes of this frame as received. */
  raw: Buffer;
}

interface Partial {
  value: RespValue;
  end: number;
}

const CR = 0x0d;
const LF = 0x0a;

const MAX_NESTING_DEPTH = 64;
const MAX_BULK_BYTES = 512 * 1024 * 1024;
const MAX_BUFFERED_BYTES = 64 * 1024 * 1024;

/**
 * Incremental RESP2 parser.
 *
 * Feed it arbitrary TCP chunks with `push`; it returns every complete frame
 * found so far and keeps the incomplete tail buffered for the next chunk.
 * An incomplete frame is re-parsed from its start when more bytes arrive,
 * which keeps the parser stateless between frames and easy to verify.
 */
export class RespParser {
  private buffer: Buffer = Buffer.alloc(0);

  push(chunk: Buffer | string): ParsedFrame[] {
    const bytes = typeof chunk === "string" ? Buffer.from(chunk, "utf8") : chunk;
    this.buffer = this.buffer.length === 0 ? bytes : Buffer.concat([this.buffer, bytes]);

    const frames: ParsedFrame[] = [];
    let offset = 0;

    while (offset < this.buffer.length) {
      const parsed = parseValue(this.buffer, offset, 0);

      if (parsed === null) {
        break;
      }

      frames.push({
        value: parsed.value,
        raw: Buffer.from(this.buffer.subarray(offset, parsed.end)),
      });
      offset = parsed.end;
    }

    this.buffer = offset === 0 ? this.buffer : Buffer.from(this.buffer.subarray(offset));

    if (this.buffer.length > MAX_BUFFERED_BYTES) {
      throw new RespProtocolError("incomplete frame exceeds buffer limit");
    }

    return frames;
  }

  /** Bytes received but not yet forming a complete frame. */
  get pendingBytes(): number {
    return this.buffer.length;
  }

  reset(): void {
    this.buffer = Buffer.alloc(0);
  }
}

/** Parses a complete RESP payload containing one or more frames. */
export function parseAll(input: Buffer | string): RespValue[] {
  const parser = new RespParser();
  const frames = parser.push(input).map((frame) => frame.value);

  if (parser.pendingBytes > 0) {
    throw new RespProtocolError("trailing incomplete frame");
  }

  return frames;
}

function readLine(buffer: Buffer, start: number): { line: string; next: number } | null {
  const cr = buffer.indexOf(CR, start);

  if (cr === -1 || cr + 1 >= buffer.length) {
    return null;
  }

  if (buffer[cr + 1] !== LF) {
    throw new RespProtocolError("expected CRLF line terminator");
  }

  return { line: buffer.toString("utf8", start, cr), next: cr + 2 };
}

function parseInteger(text: string, what: string): number {
  if (!/^-?\d+$/.test(text)) {
    throw new RespProtocolError(`invalid ${what}: ${JSON.stringify(text)}`);
  }

  return Number(text);
}

function parseValue(buffer: Buffer, offset: number, depth: number): Partial | null {
  if (offset >= buffer.length) {
    return null;
  }

  if (depth > MAX_NESTING_DEPTH) {
    throw new RespProtocolError("maximum nesting depth exceeded");
  }

  const prefix = String.fromCharCode(buffer[offset]);
  const header = readLine(buffer, offset + 1);

  if (header === null) {
    return null;
  }

  switch (prefix) {
    case "+":
      return { value: { type: "simple-string", value: header.line }, end: header.next };

    case "-":
      return { value: { type: "error", value: header.line }, end: header.next };

    case ":":
      return {
        value: { type: "integer", value: parseInteger(header.line, "integer") },
        end: header.next,
      };

    case "$": {
      const length = parseInteger(header.line, "bulk length");

      if (length === -1) {
        return { value: { type: "null-bulk-string" }, end: header.next };
      }

      if (length < -1 || length > MAX_BULK_BYTES) {
        throw new RespProtocolError(`invalid bulk length ${length}`);
      }

      const end = header.next + length;

      if (end + 2 > buffer.length) {
        return null;
      }

      if (buffer[end] !== CR || buffer[end + 1] !== LF) {
        throw new RespProtocolError("bulk string not terminated by CRLF");
      }

      return {
        value: { type: "bulk-string", value: buffer.toString("utf8", header.next, end) },
        end: end + 2,
      };
    }

    case "*": {
      const count = parseInteger(header.line, "array length");

      if (count === -1) {
        return { value: { type: "null-array" }, end: header.next };
      }

      if (count < -1) {
        throw new RespProtocolError(`invalid array length ${count}`);
      }

      const items: RespValue[] = [];
      let cursor = header.next;

      for (let index = 0; index < count; index += 1) {
        const item = parseValue(buffer, cursor, depth + 1);

        if (item === null) {
          return null;
        }

        items.push(item.value);
        cursor = item.end;
      }

      return { value: { type: "array", value: items }, end: cursor };
    }

    default:
      throw new RespProtocolError(`unknown RESP type prefix ${JSON.stringify(prefix)}`);
  }
}
