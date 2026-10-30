export class TokenizeError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "TokenizeError";
  }
}

const ESCAPES: Record<string, string> = {
  n: "\n",
  r: "\r",
  t: "\t",
  b: "\b",
  a: "\x07",
  '"': '"',
  "\\": "\\",
};

/**
 * Splits a command line into arguments with redis-cli style quoting:
 *
 *   SET message "hello world"      -> ["SET", "message", "hello world"]
 *   SET k 'it''s'                  -> single quotes are literal, \' escapes a quote
 *   SET k "line\nbreak \x41"       -> double quotes support \n \r \t \" \\ \xHH
 */
export function tokenize(input: string): string[] {
  const args: string[] = [];
  let index = 0;

  while (index < input.length) {
    while (index < input.length && /\s/.test(input[index])) index += 1;
    if (index >= input.length) break;

    let current = "";
    let quoted = false;

    while (index < input.length && !/\s/.test(input[index])) {
      const char = input[index];

      if (char === '"') {
        quoted = true;
        index += 1;
        let closed = false;

        while (index < input.length) {
          const inner = input[index];

          if (inner === "\\" && index + 1 < input.length) {
            const next = input[index + 1];

            if (next === "x" && /^[0-9a-fA-F]{2}$/.test(input.slice(index + 2, index + 4))) {
              current += String.fromCharCode(parseInt(input.slice(index + 2, index + 4), 16));
              index += 4;
              continue;
            }

            current += ESCAPES[next] ?? next;
            index += 2;
            continue;
          }

          if (inner === '"') {
            closed = true;
            index += 1;
            break;
          }

          current += inner;
          index += 1;
        }

        if (!closed) throw new TokenizeError("unbalanced double quotes");
        continue;
      }

      if (char === "'") {
        quoted = true;
        index += 1;
        let closed = false;

        while (index < input.length) {
          const inner = input[index];

          if (inner === "\\" && input[index + 1] === "'") {
            current += "'";
            index += 2;
            continue;
          }

          if (inner === "'") {
            closed = true;
            index += 1;
            break;
          }

          current += inner;
          index += 1;
        }

        if (!closed) throw new TokenizeError("unbalanced single quotes");
        continue;
      }

      current += char;
      index += 1;
    }

    if (current !== "" || quoted) args.push(current);
  }

  return args;
}
