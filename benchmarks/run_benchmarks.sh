#!/usr/bin/env bash

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-6379}"
REQUESTS="${3:-100000}"
CLIENTS="${4:-50}"
PIPELINE="${5:-16}"

if ! command -v redis-benchmark >/dev/null 2>&1; then
    echo "redis-benchmark is required"
    exit 1
fi

if ! redis-cli -h "$HOST" -p "$PORT" PING >/dev/null 2>&1; then
    echo "Redis server is not reachable at $HOST:$PORT"
    exit 1
fi

mkdir -p benchmark_results

RESULT_FILE="benchmark_results/benchmark_$(date +%Y%m%d_%H%M%S).txt"

VALUE="$(printf 'x%.0s' {1..64})"

{
    echo "Redis From Scratch Benchmark"
    echo
    echo "Host: $HOST"
    echo "Port: $PORT"
    echo "Requests: $REQUESTS"
    echo "Clients: $CLIENTS"
    echo "Pipeline: $PIPELINE"
    echo

    echo "System:"
    uname -a
    echo

    echo "Compiler:"
    c++ --version | head -n 1
    echo

    echo "=== PING ==="

    redis-benchmark \
        -h "$HOST" \
        -p "$PORT" \
        -n "$REQUESTS" \
        -c "$CLIENTS" \
        -P "$PIPELINE" \
        -q \
        PING

    echo
    echo "=== SET 64-byte value ==="

    redis-benchmark \
        -h "$HOST" \
        -p "$PORT" \
        -n "$REQUESTS" \
        -c "$CLIENTS" \
        -P "$PIPELINE" \
        -r 100000 \
        -q \
        SET bench:__rand_int__ "$VALUE"

    echo
    echo "=== GET ==="

    redis-cli \
        -h "$HOST" \
        -p "$PORT" \
        SET benchmark:key "$VALUE" \
        >/dev/null

    redis-benchmark \
        -h "$HOST" \
        -p "$PORT" \
        -n "$REQUESTS" \
        -c "$CLIENTS" \
        -P "$PIPELINE" \
        -q \
        GET benchmark:key

    echo
    echo "=== INFO stats ==="

    redis-cli \
        -h "$HOST" \
        -p "$PORT" \
        INFO stats

} | tee "$RESULT_FILE"

echo
echo "Results saved to:"
echo "$RESULT_FILE"