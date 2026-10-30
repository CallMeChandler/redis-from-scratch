import { describe, expect, it } from "vitest";

import { completeCommand, hintFor } from "../src/lib/commands";
import { parseInfo, parseKeyspace } from "../src/lib/info";
import { formatCli, toPlain } from "../src/lib/resp/format";
import { parseAll } from "../src/lib/resp/parser";
import { TokenizeError, tokenize } from "../src/lib/resp/tokenize";

describe("tokenize", () => {
  it("splits on whitespace", () => {
    expect(tokenize("  SET user   ritvik ")).toEqual(["SET", "user", "ritvik"]);
  });

  it("supports double quotes with escapes", () => {
    expect(tokenize('SET message "hello world"')).toEqual(["SET", "message", "hello world"]);
    expect(tokenize('HSET user name "Ritvik Mod"')).toEqual(["HSET", "user", "name", "Ritvik Mod"]);
    expect(tokenize('SET k "a\\nb \\"q\\" \\x41"')).toEqual(["SET", "k", 'a\nb "q" A']);
  });

  it("supports single quotes and empty strings", () => {
    expect(tokenize("SET k 'it\\'s raw \\n'")).toEqual(["SET", "k", "it's raw \\n"]);
    expect(tokenize('SET k ""')).toEqual(["SET", "k", ""]);
  });

  it("rejects unbalanced quotes", () => {
    expect(() => tokenize('SET k "oops')).toThrow(TokenizeError);
  });
});

describe("formatCli", () => {
  it("formats like redis-cli", () => {
    const [ok, str, int, nil, arr, nested, empty] = parseAll(
      '+OK\r\n$6\r\nritvik\r\n:3\r\n$-1\r\n*2\r\n$1\r\na\r\n$1\r\nb\r\n*2\r\n+OK\r\n*2\r\n:1\r\n:2\r\n*0\r\n',
    );

    expect(formatCli(ok)).toBe("OK");
    expect(formatCli(str)).toBe('"ritvik"');
    expect(formatCli(int)).toBe("(integer) 3");
    expect(formatCli(nil)).toBe("(nil)");
    expect(formatCli(arr)).toBe('1) "a"\n2) "b"');
    expect(formatCli(nested)).toBe("1) OK\n2) 1) (integer) 1\n   2) (integer) 2");
    expect(formatCli(empty)).toBe("(empty array)");
  });

  it("maps to plain values", () => {
    expect(toPlain(parseAll("*3\r\n:1\r\n$-1\r\n$1\r\nx\r\n")[0])).toEqual([1, null, "x"]);
  });
});

describe("parseInfo", () => {
  it("parses sections and keyspace", () => {
    const sections = parseInfo(
      "# Server\r\nredis_version:redis-from-scratch\r\nuptime_in_seconds:12\r\n\r\n# Keyspace\r\ndb0:keys=3,expires=1\r\n",
    );

    expect(sections.server.uptime_in_seconds).toBe("12");
    expect(parseKeyspace(sections.keyspace.db0)).toEqual({ keys: 3, expires: 1 });
  });
});

describe("command hints", () => {
  it("hints remaining arguments", () => {
    expect(hintFor("set", 0)).toBe("key value");
    expect(hintFor("SET", 1)).toBe("value");
    expect(hintFor("SET", 2)).toBe("");
    expect(hintFor("DEL", 3)).toBe("");
    expect(hintFor("nope", 0)).toBe("");
  });

  it("completes command names", () => {
    expect(completeCommand("ex")).toEqual(["EXISTS", "EXPIRE", "EXPIREAT", "EXEC"]);
  });
});
