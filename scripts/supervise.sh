#!/usr/bin/env bash
# Supervisor ringan untuk instans LAST STAND.
#
#   ./scripts/supervise.sh <nama> [interval_detik]
#
# Kenapa bukan cron/watchdog.sh atau systemd --user:
# di mesin ini samp03svr (32-bit) dibunuh seccomp dengan SIGSYS bila dijalankan
# dari cron SYSTEMD/cron sandbox (bukti: backups/watchdog.log "Bad system call",
# journalctl --user "status=31/SYS"). Proses yang dijalankan dari sesi shell
# (termasuk turunannya) berjalan normal, jadi supervisor ini harus dijalankan
# dari sesi yang sama:
#
#   setsid nohup ./scripts/supervise.sh s1 >> backups/supervisor.log 2>&1 &
#
# Supervisor hanya menyalakan ulang bila proses mati; ia tidak menyentuh proses
# yang hidup, dan menulis satu baris log per kejadian.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="${1:-s1}"
INTERVAL="${2:-10}"
DIR="$ROOT/runtime/$NAME"
LOG="$ROOT/backups/supervisor.log"

[[ -d "$DIR" ]] || { echo "Instans '$NAME' tidak ada." >&2; exit 1; }
mkdir -p "$ROOT/backups"

# Batas RSS: proses 32-bit di mesin ini SIGSEGV di _IO_vsprintf saat RSS
# menembus ~1,2-1,5 GB (3 dump sama: 13:57, 14:27, 14:43). Daripada menunggu
# crash, restart terencana lebih dulu supaya pemain tersimpan rapi.
MEM_LIMIT_MB="${MEM_LIMIT_MB:-1200}"

alive() {
  local pid
  pid="$(cat "$DIR/server.pid" 2>/dev/null || echo 0)"
  [[ -n "$pid" && "$pid" != "0" ]] || return 1
  kill -0 "$pid" 2>/dev/null
}

note() { printf '[%s] supervisor %s: %s\n' "$(date '+%F %T')" "$NAME" "$1" >> "$LOG"; }

note "mulai (interval ${INTERVAL}s, pid supervisor $$)"
restarts=0

while :; do
  if alive; then
    rss_mb="$(ps -o rss= -p "$(cat "$DIR/server.pid")" 2>/dev/null | awk '{print int($1/1024)}')"
    if [[ -n "${rss_mb:-}" && "$rss_mb" -ge "$MEM_LIMIT_MB" ]]; then
      note "RSS ${rss_mb} MB >= batas ${MEM_LIMIT_MB} MB, restart terencana"
      "$ROOT/scripts/instance.sh" stop "$NAME" >> "$LOG" 2>&1 || true
      sleep 2
      "$ROOT/scripts/instance.sh" start "$NAME" >> "$LOG" 2>&1 || note "restart terencana gagal"
    fi
  else
    restarts=$((restarts + 1))
    note "proses mati, restart #$restarts"
    if ! "$ROOT/scripts/instance.sh" start "$NAME" >> "$LOG" 2>&1; then
      note "restart gagal, coba lagi ${INTERVAL}s"
    fi
  fi
  sleep "$INTERVAL"
done
