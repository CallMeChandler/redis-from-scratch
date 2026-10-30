# Redis From Scratch C++ server. Builds the existing sources unchanged with CMake
# and runs redis-server with a config file.
#
#   docker build -t redis-from-scratch .
#   docker run -p 6379:6379 -v rfs-data:/data redis-from-scratch
#   docker run ... redis-from-scratch redis-server /etc/redis-from-scratch/replica.conf

FROM debian:bookworm-slim AS build
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

FROM debian:bookworm-slim AS runtime
COPY --from=build /src/build/redis-server /usr/local/bin/redis-server
COPY docker/primary.conf docker/replica.conf /etc/redis-from-scratch/
RUN useradd --system --create-home --home-dir /data redis
USER redis
# rdb-file / aof-file in the configs are relative, so they land in /data.
WORKDIR /data
VOLUME /data
EXPOSE 6379
CMD ["redis-server", "/etc/redis-from-scratch/primary.conf"]
