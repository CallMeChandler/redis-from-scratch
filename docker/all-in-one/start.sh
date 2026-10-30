#!/usr/bin/env bash
# Starts primary, replica and dashboard; stops everything if any one exits,
# and forwards SIGTERM so the servers can write their shutdown snapshot.
set -euo pipefail

wait_for_port() {
  for _ in $(seq 1 100); do
    if (exec 3<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null; then return 0; fi
    sleep 0.1
  done
  echo "port $1 did not open" >&2
  return 1
}

(cd /data/primary && exec redis-server /etc/redis-from-scratch/primary.conf) &
primary=$!
wait_for_port 6379

(cd /data/replica && exec redis-server /etc/redis-from-scratch/replica.conf) &
replica=$!
wait_for_port 6380

node /app/dist/server.js &
dashboard=$!

shutdown() {
  kill -TERM "$dashboard" "$replica" "$primary" 2>/dev/null || true
  wait || true
}
trap 'shutdown; exit 0' TERM INT

wait -n
echo "a process exited; stopping the container" >&2
shutdown
exit 1
