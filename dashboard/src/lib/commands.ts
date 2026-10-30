/**
 * Commands implemented by the C++ server, with the exact arity its handlers
 * accept (from src/command/command_handler.cpp and src/net/tcp_server.cpp).
 * Used for CLI hints, completion and `help`.
 */
export interface CommandDoc {
  name: string;
  args: string;
  group: string;
  summary: string;
}

export const COMMANDS: CommandDoc[] = [
  { name: "PING", args: "[message]", group: "basic", summary: "PONG, or echo the message" },
  { name: "ECHO", args: "message", group: "basic", summary: "Return the message" },
  { name: "EXISTS", args: "key [key ...]", group: "keys", summary: "Count existing keys" },
  { name: "DEL", args: "key [key ...]", group: "keys", summary: "Delete keys" },
  { name: "SET", args: "key value", group: "strings", summary: "Set a string (no options)" },
  { name: "GET", args: "key", group: "strings", summary: "Get a string" },
  { name: "HSET", args: "key field value", group: "hashes", summary: "Set one hash field" },
  { name: "HGET", args: "key field", group: "hashes", summary: "Get one hash field" },
  { name: "LPUSH", args: "key value", group: "lists", summary: "Prepend one value" },
  { name: "RPUSH", args: "key value", group: "lists", summary: "Append one value" },
  { name: "LPOP", args: "key", group: "lists", summary: "Pop the head" },
  { name: "RPOP", args: "key", group: "lists", summary: "Pop the tail" },
  { name: "LRANGE", args: "key start stop", group: "lists", summary: "Inclusive range" },
  { name: "LLEN", args: "key", group: "lists", summary: "List length" },
  { name: "SADD", args: "key member", group: "sets", summary: "Add one member" },
  { name: "SREM", args: "key member", group: "sets", summary: "Remove one member" },
  { name: "SISMEMBER", args: "key member", group: "sets", summary: "Membership test" },
  { name: "SMEMBERS", args: "key", group: "sets", summary: "All members" },
  { name: "ZADD", args: "key score member", group: "sorted sets", summary: "Add one scored member" },
  { name: "ZSCORE", args: "key member", group: "sorted sets", summary: "Score of a member" },
  { name: "ZRANGE", args: "key start stop", group: "sorted sets", summary: "Members by rank" },
  { name: "EXPIRE", args: "key seconds", group: "expiry", summary: "Relative TTL" },
  { name: "EXPIREAT", args: "key unix-seconds", group: "expiry", summary: "Absolute expiry" },
  { name: "TTL", args: "key", group: "expiry", summary: "Seconds left (-1 none, -2 missing)" },
  { name: "MULTI", args: "", group: "transactions", summary: "Start queuing" },
  { name: "EXEC", args: "", group: "transactions", summary: "Run the queue" },
  { name: "DISCARD", args: "", group: "transactions", summary: "Drop the queue" },
  { name: "WATCH", args: "key [key ...]", group: "transactions", summary: "Optimistic lock" },
  { name: "UNWATCH", args: "", group: "transactions", summary: "Forget watched keys" },
  { name: "SUBSCRIBE", args: "channel [channel ...]", group: "pub/sub", summary: "Enter subscribe mode" },
  { name: "UNSUBSCRIBE", args: "[channel ...]", group: "pub/sub", summary: "Leave channels (all if none)" },
  { name: "PUBLISH", args: "channel message", group: "pub/sub", summary: "Send a message" },
  { name: "SAVE", args: "", group: "persistence", summary: "Write an RDB snapshot" },
  { name: "REWRITEAOF", args: "", group: "persistence", summary: "Compact the AOF" },
  { name: "MEMORY", args: "USAGE key", group: "memory", summary: "Logical bytes of a key" },
  { name: "INFO", args: "[server|clients|memory|stats|replication|keyspace]", group: "observability", summary: "Runtime metrics" },
];

const BY_NAME = new Map(COMMANDS.map((command) => [command.name, command]));

export function findCommand(name: string): CommandDoc | undefined {
  return BY_NAME.get(name.toUpperCase());
}

/** Remaining argument placeholders after `typed` arguments, redis-cli style. */
export function hintFor(name: string, typedArgs: number): string {
  const doc = findCommand(name);
  if (!doc || !doc.args) return "";

  const tokens = doc.args.match(/\[[^\]]*\]|\S+/g) ?? [];
  const variadic = tokens.findIndex((token) => token.includes("..."));
  if (variadic !== -1 && typedArgs > variadic) return "";
  return tokens.slice(typedArgs).join(" ");
}

export function completeCommand(prefix: string): string[] {
  const upper = prefix.toUpperCase();
  return COMMANDS.map((command) => command.name).filter((name) => name.startsWith(upper));
}
