#!/usr/bin/env bash
# Watchdog: kalau proses instans mati (FCNPC segfault, OOM, crash plugin),
# hidupkan lagi. Dipanggil cron tiap menit. Log satu baris per kejadian.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="${1:-s1}"
DIR="$ROOT/runtime/$NAME"
LOG="$ROOT/backups/watchdog.log"

[[ -f "$DIR/server.pid" ]] || true

alive=0
if [[ -f "$DIR/server.pid" ]]; then
  pid="$(cat "$DIR/server.pid" 2>/dev/null || echo 0)"
  if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then alive=1; fi
fi

if [[ "$alive" -eq 1 ]]; then exit 0; fi

mkdir -p "$(dirname "$LOG")"
printf '[%s] instans %s mati, restart\n' "$(date '+%F %T')" "$NAME" >> "$LOG"
"$ROOT/scripts/instance.sh" start "$NAME" >> "$LOG" 2>&1
