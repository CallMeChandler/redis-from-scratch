#!/usr/bin/env bash

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-6379}"

redis_cmd() {
    redis-cli \
        --raw \
        -h "$HOST" \
        -p "$PORT" \
        "$@"
}

fail() {
    echo "FAIL: $1"
    exit 1
}

echo "Running Redis From Scratch smoke tests..."

[[ "$(redis_cmd PING)" == "PONG" ]] ||
    fail "PING"

[[ "$(redis_cmd SET smoke:string hello)" == "OK" ]] ||
    fail "SET"

[[ "$(redis_cmd GET smoke:string)" == "hello" ]] ||
    fail "GET"

redis_cmd HSET smoke:hash field value >/dev/null

[[ "$(redis_cmd HGET smoke:hash field)" == "value" ]] ||
    fail "HASH"

redis_cmd DEL smoke:list >/dev/null
redis_cmd RPUSH smoke:list one >/dev/null
redis_cmd RPUSH smoke:list two >/dev/null

LIST_OUTPUT="$(redis_cmd LRANGE smoke:list 0 -1)"

[[ "$LIST_OUTPUT" == $'one\ntwo' ]] ||
    fail "LIST"

redis_cmd SADD smoke:set cpp >/dev/null

[[ "$(redis_cmd SISMEMBER smoke:set cpp)" == "1" ]] ||
    fail "SET TYPE"

redis_cmd ZADD smoke:zset 10 alice >/dev/null

[[ "$(redis_cmd ZSCORE smoke:zset alice)" != "" ]] ||
    fail "SORTED SET"

redis_cmd SET smoke:ttl value >/dev/null
redis_cmd EXPIRE smoke:ttl 30 >/dev/null

TTL="$(redis_cmd TTL smoke:ttl)"

if [[ "$TTL" -le 0 ]]; then
    fail "TTL"
fi

INFO_OUTPUT="$(redis_cmd INFO)"

grep -q "# Server" <<< "$INFO_OUTPUT" ||
    fail "INFO server"

grep -q "# Memory" <<< "$INFO_OUTPUT" ||
    fail "INFO memory"

grep -q "# Replication" <<< "$INFO_OUTPUT" ||
    fail "INFO replication"

redis_cmd DEL \
    smoke:string \
    smoke:hash \
    smoke:list \
    smoke:set \
    smoke:zset \
    smoke:ttl \
    >/dev/null

echo "All smoke tests passed."