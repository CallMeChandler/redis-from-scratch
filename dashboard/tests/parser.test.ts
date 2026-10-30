import { describe, expect, it } from "vitest";

import { encodeCommand, encodePipeline } from "../src/lib/resp/encoder";
import { RespParser, RespProtocolError, parseAll } from "../src/lib/resp/parser";
import type { RespValue } from "../src/lib/resp/types";

function one(input: string): RespValue {
  const values = parseAll(input);
  expect(values).toHaveLength(1);
  return values[0];
}

describe("RespParser: scalar types", () => {
  it("parses simple strings", () => {
    expect(one("+OK\r\n")).toEqual({ type: "simple-string", value: "OK" });
  });

  it("parses errors", () => {
    expect(one("-ERR something\r\n")).toEqual({ type: "error", value: "ERR something" });
    expect(one("-READONLY You can't write against a read only replica.\r\n")).toEqual({
      type: "error",
      value: "READONLY You can't write against a read only replica.",
    });
  });

  it("parses integers, including negatives", () => {
    expect(one(":123\r\n")).toEqual({ type: "integer", value: 123 });
    expect(one(":-2\r\n")).toEqual({ type: "integer", value: -2 });
  });

  it("parses bulk strings", () => {
    expect(one("$5\r\nhello\r\n")).toEqual({ type: "bulk-string", value: "hello" });
  });

  it("parses empty bulk strings", () => {
    expect(one("$0\r\n\r\n")).toEqual({ type: "bulk-string", value: "" });
  });

  it("parses bulk strings that contain CRLF", () => {
    expect(one("$7\r\na\r\nb\r\nc\r\n")).toEqual({ type: "bulk-string", value: "a\r\nb\r\nc" });
  });

  it("uses byte lengths for multi-byte UTF-8", () => {
    // "héllo" is 6 bytes in UTF-8
    expect(one("$6\r\nhéllo\r\n")).toEqual({ type: "bulk-string", value: "héllo" });
  });

  it("parses null bulk strings", () => {
    expect(one("$-1\r\n")).toEqual({ type: "null-bulk-string" });
  });
});

describe("RespParser: arrays", () => {
  it("parses flat arrays", () => {
    expect(one("*3\r\n$3\r\nfoo\r\n:1\r\n+OK\r\n")).toEqual({
      type: "array",
      value: [
        { type: "bulk-string", value: "foo" },
        { type: "integer", value: 1 },
        { type: "simple-string", value: "OK" },
      ],
    });
  });

  it("parses empty and null arrays", () => {
    expect(one("*0\r\n")).toEqual({ type: "array", value: [] });
    expect(one("*-1\r\n")).toEqual({ type: "null-array" });
  });

  it("parses nested arrays (e.g. EXEC results)", () => {
    expect(one("*2\r\n+OK\r\n*2\r\n$1\r\na\r\n$-1\r\n")).toEqual({
      type: "array",
      value: [
        { type: "simple-string", value: "OK" },
        {
          type: "array",
          value: [{ type: "bulk-string", value: "a" }, { type: "null-bulk-string" }],
        },
      ],
    });
  });

  it("parses arrays containing errors", () => {
    expect(one("*2\r\n:1\r\n-WRONGTYPE bad\r\n")).toEqual({
      type: "array",
      value: [
        { type: "integer", value: 1 },
        { type: "error", value: "WRONGTYPE bad" },
      ],
    });
  });
});

describe("RespParser: streaming", () => {
  it("waits for fragmented responses and completes them", () => {
    const parser = new RespParser();
    expect(parser.push("$5\r\nhel")).toEqual([]);
    expect(parser.pendingBytes).toBe(7);

    const frames = parser.push("lo\r\n");
    expect(frames.map((frame) => frame.value)).toEqual([{ type: "bulk-string", value: "hello" }]);
    expect(frames[0].raw.toString()).toBe("$5\r\nhello\r\n");
    expect(parser.pendingBytes).toBe(0);
  });

  it("handles a split between CR and LF", () => {
    const parser = new RespParser();
    expect(parser.push("+OK\r")).toEqual([]);
    expect(parser.push("\n").map((frame) => frame.value)).toEqual([{ type: "simple-string", value: "OK" }]);
  });

  it("produces the same result for every possible split point", () => {
    const input = "*3\r\n$9\r\nsubscribe\r\n$4\r\nnews\r\n:1\r\n";
    const expected = parseAll(input);

    for (let split = 1; split < input.length; split += 1) {
      const parser = new RespParser();
      const values = [...parser.push(input.slice(0, split)), ...parser.push(input.slice(split))].map(
        (frame) => frame.value,
      );
      expect(values).toEqual(expected);
    }
  });

  it("handles byte-by-byte delivery", () => {
    const input = Buffer.from("*2\r\n$3\r\nfoo\r\n*1\r\n:42\r\n");
    const parser = new RespParser();
    const values: RespValue[] = [];

    for (const byte of input) values.push(...parser.push(Buffer.from([byte])).map((frame) => frame.value));

    expect(values).toHaveLength(1);
    expect(parser.pendingBytes).toBe(0);
  });

  it("splits multiple responses arriving in one TCP chunk", () => {
    const parser = new RespParser();
    const frames = parser.push("+OK\r\n:2\r\n$-1\r\n*1\r\n$1\r\nx\r\n+PART");

    expect(frames.map((frame) => frame.value)).toEqual([
      { type: "simple-string", value: "OK" },
      { type: "integer", value: 2 },
      { type: "null-bulk-string" },
      { type: "array", value: [{ type: "bulk-string", value: "x" }] },
    ]);
    expect(frames.map((frame) => frame.raw.toString())).toEqual(["+OK\r\n", ":2\r\n", "$-1\r\n", "*1\r\n$1\r\nx\r\n"]);
    expect(parser.pendingBytes).toBe(5);
    expect(parser.push("IAL\r\n").map((frame) => frame.value)).toEqual([{ type: "simple-string", value: "PARTIAL" }]);
  });
});

describe("RespParser: malformed input", () => {
  it("rejects unknown type prefixes", () => {
    expect(() => parseAll("!oops\r\n")).toThrow(RespProtocolError);
  });

  it("rejects non-numeric lengths", () => {
    expect(() => parseAll("$abc\r\n")).toThrow(RespProtocolError);
    expect(() => parseAll(":12x\r\n")).toThrow(RespProtocolError);
  });

  it("rejects bulk strings without a trailing CRLF", () => {
    expect(() => parseAll("$3\r\nfooXY")).toThrow(RespProtocolError);
  });

  it("rejects bare CR inside a line", () => {
    expect(() => parseAll("+O\rK\r\n")).toThrow(RespProtocolError);
  });
});

describe("encodeCommand", () => {
  it("encodes arrays of bulk strings", () => {
    expect(encodeCommand(["SET", "user", "ritvik"]).toString()).toBe(
      "*3\r\n$3\r\nSET\r\n$4\r\nuser\r\n$6\r\nritvik\r\n",
    );
  });

  it("uses UTF-8 byte lengths and handles empty arguments", () => {
    expect(encodeCommand(["SET", "k", "é", ""]).toString()).toBe("*4\r\n$3\r\nSET\r\n$1\r\nk\r\n$2\r\né\r\n$0\r\n\r\n");
  });

  it("round-trips through the parser", () => {
    const args = ["HSET", "user", "name", "Ritvik Mod\r\nline two"];
    expect(parseAll(encodeCommand(args))).toEqual([
      { type: "array", value: args.map((value) => ({ type: "bulk-string", value })) },
    ]);
  });

  it("pipelines commands into one buffer", () => {
    expect(parseAll(encodePipeline([["PING"], ["GET", "a"]]))).toHaveLength(2);
  });
});
