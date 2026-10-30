# Dashboard + Node RESP bridge as one long-lived Node service (Railway, Fly, any container host).
# Build context: dashboard/
#
#   docker build -t rfs-dashboard dashboard
#   docker run -p 3000:3000 -e REDIS_HOST=host.docker.internal rfs-dashboard

FROM node:22-bookworm-slim AS build
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
RUN npm run build && npm prune --omit=dev

FROM node:22-bookworm-slim AS runtime
ENV NODE_ENV=production \
    PORT=3000 \
    NEXT_TELEMETRY_DISABLED=1
WORKDIR /app
COPY --from=build --chown=node:node /app/package.json ./
COPY --from=build --chown=node:node /app/node_modules ./node_modules
COPY --from=build --chown=node:node /app/.next ./.next
COPY --from=build --chown=node:node /app/dist ./dist
COPY --from=build --chown=node:node /app/public ./public
COPY --from=build --chown=node:node /app/next.config.ts ./
USER node
EXPOSE 3000
# Serves HTTP, /api/* and the /ws WebSocket bridge on $PORT.
CMD ["node", "dist/server.js"]
