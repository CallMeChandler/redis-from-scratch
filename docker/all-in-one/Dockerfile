# Single-service image for Railway (or any host without stable private IPv4):
# C++ primary + replica + dashboard in one container, talking over 127.0.0.1.
# Only the dashboard's $PORT is exposed; Redis ports stay internal.
#
# Build context: repository root
#   docker build -f docker/all-in-one/Dockerfile -t rfs-all-in-one .

FROM debian:bookworm-slim AS cpp
RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ make cmake \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY include include
COPY src src
COPY tests tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target redis-server -j"$(nproc)"

FROM node:22-bookworm-slim AS dashboard
WORKDIR /app
COPY dashboard/package.json dashboard/package-lock.json ./
RUN npm ci
COPY dashboard/ ./
RUN npm run build && npm prune --omit=dev

FROM node:22-bookworm-slim AS runtime
ENV NODE_ENV=production \
    PORT=3000 \
    NEXT_TELEMETRY_DISABLED=1 \
    REDIS_HOST=127.0.0.1 \
    REDIS_PORT=6379 \
    REPLICA_HOST=127.0.0.1 \
    REPLICA_PORT=6380
COPY --from=cpp /src/build/redis-server /usr/local/bin/redis-server
COPY docker/all-in-one/primary.conf docker/all-in-one/replica.conf /etc/redis-from-scratch/
COPY --chmod=755 docker/all-in-one/start.sh /usr/local/bin/start-all
WORKDIR /app
COPY --from=dashboard /app/package.json ./
COPY --from=dashboard /app/node_modules ./node_modules
COPY --from=dashboard /app/.next ./.next
COPY --from=dashboard /app/dist ./dist
COPY --from=dashboard /app/public ./public
COPY --from=dashboard /app/next.config.ts ./
RUN mkdir -p /data/primary /data/replica && chown -R node:node /data /app
USER node
EXPOSE 3000
CMD ["start-all"]
