#!/usr/bin/env bash
# Probe memori: berapa MB yang ditambahkan per pemain, dan apakah ada
# kebocoran per detik saat pemain idle.
#
#   tools/loadtest/mem_probe.sh <bot> <detik> [restart:1|0]
#
# Contoh:
#   tools/loadtest/mem_probe.sh 10 90 1
#   tools/loadtest/mem_probe.sh 60 90 1
#
# Keluaran: deret RSS tiap 10 dtk + ringkasan (MB/pemain, MB/detik saat idle).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BOTS="${1:-10}"
SECONDS_RUN="${2:-90}"
DO_RESTART="${3:-1}"
INSTANCE="${INSTANCE:-s1}"
DIR="$ROOT/runtime/$INSTANCE"
SAMPLE=10

cd "$ROOT"

if [[ "$DO_RESTART" == "1" ]]; then
  ./scripts/instance.sh restart "$INSTANCE" >/dev/null
  echo "menunggu boot..."
  sleep 35
fi

PID="$(cat "$DIR/server.pid")"
kill -0 "$PID" 2>/dev/null || { echo "server tidak jalan" >&2; exit 1; }

baseline_mb=$(( $(ps -o rss= -p "$PID" | tr -d ' ') / 1024 ))
echo "bot=$BOTS durasi=${SECONDS_RUN}s baseline=${baseline_mb} MB pid=$PID"

python3 tools/loadtest/bot_run.py --count "$BOTS" --duration "$((SECONDS_RUN + 20))" \
    --report "/tmp/mem_probe_bots.json" > /tmp/mem_probe_bots.log 2>&1 &
BOT_PID=$!
trap 'kill "$BOT_PID" 2>/dev/null || true' EXIT

series=()
for (( t=0; t<=SECONDS_RUN; t+=SAMPLE )); do
  if ! kill -0 "$PID" 2>/dev/null; then
    echo "SERVER MATI pada detik $t"
    break
  fi
  rss_mb=$(( $(ps -o rss= -p "$PID" | tr -d ' ') / 1024 ))
  series+=("$t:$rss_mb")
  printf '%4ds  %5d MB\n' "$t" "$rss_mb"
  sleep "$SAMPLE"
done

kill "$BOT_PID" 2>/dev/null || true

python3 - "$BOTS" "$baseline_mb" "${series[@]}" <<'PY'
import sys
bots = int(sys.argv[1]); base = int(sys.argv[2])
pts = [tuple(map(int, s.split(':'))) for s in sys.argv[3:]]
if len(pts) < 2:
    print("sampel kurang"); raise SystemExit
peak = max(v for _, v in pts)
last_t, last_v = pts[-1]
idle_window = [p for p in pts if p[0] >= last_t * 0.6]
slope = 0.0
if len(idle_window) >= 2:
    (t0, v0), (t1, v1) = idle_window[0], idle_window[-1]
    slope = (v1 - v0) / max(t1 - t0, 1)
print(f"\nringkasan: baseline {base} MB, puncak {peak} MB, delta {peak - base} MB")
print(f"biaya per pemain : {(peak - base) / bots:.2f} MB")
print(f"laju saat idle   : {slope:.2f} MB/detik (10 dtk terakhir)")
PY
