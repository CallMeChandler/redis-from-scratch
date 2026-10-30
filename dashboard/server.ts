import { createServer } from "node:http";

import next from "next";

import { getEndpoint } from "./src/lib/bridge/config";
import { WS_PATH, createBridgeWebSocketServer } from "./src/lib/bridge/ws-server";

/**
 * Long-lived Node process serving the Next.js app plus the WebSocket
 * bridge used for sessions (transactions, Pub/Sub, Raw CLI).
 * Railway (or any container host) runs this directly; no serverless.
 */
const dev = process.argv.includes("--dev");
const hostname = process.env.HOSTNAME_BIND ?? "0.0.0.0";
const port = Number(process.env.PORT) || 3000;

async function main(): Promise<void> {
  const app = next({ dev, hostname, port });
  const handle = app.getRequestHandler();
  await app.prepare();

  const bridge = createBridgeWebSocketServer();

  const server = createServer((request, response) => {
    void handle(request, response);
  });

  // Only /ws is ours. Next.js attaches its own upgrade listener to this
  // server on the first request (dev HMR) and ignores paths it doesn't own,
  // so other upgrades are deliberately left to it.
  server.on("upgrade", (request, socket, head) => {
    const { pathname } = new URL(request.url ?? "/", "http://localhost");
    if (pathname === WS_PATH) bridge.handleUpgrade(request, socket, head);
  });

  server.listen(port, hostname, () => {
    const primary = getEndpoint("primary");
    const replica = getEndpoint("replica");
    console.log(`> Redis From Scratch dashboard on http://${hostname}:${port} (${dev ? "dev" : "production"})`);
    console.log(`> primary ${primary ? `${primary.host}:${primary.port}` : "not configured"}`);
    console.log(`> replica ${replica ? `${replica.host}:${replica.port}` : "not configured"}`);
  });

  const shutdown = () => {
    bridge.close();
    server.close(() => process.exit(0));
    setTimeout(() => process.exit(0), 3000).unref();
  };

  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}

main().catch((error: unknown) => {
  console.error(error);
  process.exit(1);
});
