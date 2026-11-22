#!/usr/bin/env bash
# Soak test: beban bot + sampling metrik server.
#
#   ./scripts/soak.sh [menit] [jumlah_bot] [nama_instans]
#
# Contoh: ./scripts/soak.sh 60 100 s1
#
# Hasil di runtime/soak/<stempel>/:
#   samples.csv   satu baris per 30 detik (RSS, zombie, PERF, error counter)
#   summary.md    ringkasan + gate PASS/FAIL
#   bots.json     laporan bot_run.py
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MINUTES="${1:-60}"
BOTS="${2:-100}"
NAME="${3:-s1}"
DIR="$ROOT/runtime/$NAME"
STAMP="$(date '+%Y%m%d-%H%M%S')"
OUT="$ROOT/runtime/soak/$STAMP"
SAMPLE_SECONDS="${SOAK_SAMPLE_SECONDS:-30}"

[[ -d "$DIR" ]] || { echo "Instans '$NAME' tidak ada di $DIR" >&2; exit 1; }
PID="$(cat "$DIR/server.pid" 2>/dev/null || echo 0)"
kill -0 "$PID" 2>/dev/null || { echo "Instans '$NAME' tidak jalan (pid $PID). Nyalakan dulu." >&2; exit 1; }
command -v python3 >/dev/null || { echo "python3 wajib ada untuk bot beban." >&2; exit 1; }

mkdir -p "$OUT"
LOG="$DIR/server_log.txt"
START_LINE="$(wc -l < "$LOG")"
START_ERR="$(wc -l < "$ROOT/logs/errors.log" 2>/dev/null || echo 0)"
DURATION=$((MINUTES * 60))
END_AT=$(( $(date +%s) + DURATION ))

echo "== SOAK $MINUTES menit, $BOTS bot, instans $NAME (pid $PID)"
echo "== output: $OUT"

# Bot berjalan di latar; sampling di loop utama.
SPREAD="${SPREAD:-1}"
STAGGER="${STAGGER:-0.4}"
# Port dibaca dari server.cfg instans: soak di s2 (7778) tidak boleh mengirim
# 100 bot ke s1, kalau tidak hasilnya mengukur server yang salah.
PORT="$(awk '/^port /{print $2}' "$DIR/server.cfg" 2>/dev/null || echo 7777)"
PORT="${PORT:-7777}"

python3 "$ROOT/tools/loadtest/bot_run.py" --count "$BOTS" --duration "$((DURATION > 90 ? DURATION - 60 : DURATION))" \
    --port "$PORT" --spread-zones "$SPREAD" --stagger "$STAGGER" \
    --report "$OUT/bots.json" > "$OUT/bots.log" 2>&1 &
BOT_PID=$!
trap 'kill "$BOT_PID" 2>/dev/null || true' EXIT

printf 'detik,rss_mb,zombie,npc_fail,maxplayers_err,assert,errlog,perf_zombie,perf_director,perf_horde,perf_player,perf_hud\n' \
    > "$OUT/samples.csv"

elapsed=0
while :; do
  alive=0
  kill -0 "$PID" 2>/dev/null && alive=1

  rss_kb="$(ps -o rss= -p "$PID" 2>/dev/null | tr -d ' ' || true)"
  rss_mb=$(( ${rss_kb:-0} / 1024 ))

  tail -n +"$((START_LINE + 1))" "$LOG" > "$OUT/new.log" 2>/dev/null || : > "$OUT/new.log"
  zombie="$(grep -o 'total aktif [0-9]*' "$OUT/new.log" 2>/dev/null | tail -1 | awk '{print $3}' || true)"
  npc_fail="$(grep -c 'maxplayers limit\|FCNPC_Create gagal' "$OUT/new.log" || true)"
  mp_err="$(grep -c 'maxplayers limit' "$OUT/new.log" || true)"
  assert="$(grep -c 'Assertion' "$OUT/new.log" || true)"
  errlog=$(( $(wc -l < "$ROOT/logs/errors.log" 2>/dev/null || echo 0) - START_ERR ))

  perf_line="$(grep '\[PERF\]' "$OUT/new.log" 2>/dev/null | tail -1 || true)"
  p_zombie="$(awk -F'zombie=' '{split($2,a,"/"); print a[1]}' <<< "$perf_line" 2>/dev/null || true)"
  p_director="$(awk -F'director=' '{split($2,a,"/"); print a[1]}' <<< "$perf_line" 2>/dev/null || true)"
  p_horde="$(awk -F'horde=' '{split($2,a,"/"); print a[1]}' <<< "$perf_line" 2>/dev/null || true)"
  p_player="$(awk -F'player=' '{split($2,a,"/"); print a[1]}' <<< "$perf_line" 2>/dev/null || true)"
  p_hud="$(awk -F'hud=' '{split($2,a,"/"); print a[1]}' <<< "$perf_line" 2>/dev/null || true)"

  printf '%d,%d,%s,%s,%s,%s,%d,%s,%s,%s,%s,%s\n' \
      "$elapsed" "$rss_mb" "${zombie:-0}" "${npc_fail:-0}" "${mp_err:-0}" "${assert:-0}" "$errlog" \
      "${p_zombie:-0}" "${p_director:-0}" "${p_horde:-0}" "${p_player:-0}" "${p_hud:-0}" >> "$OUT/samples.csv"

  if [[ "$alive" -eq 0 ]]; then
    echo "[$(date '+%T')] SERVER MATI pada detik $elapsed" | tee -a "$OUT/summary.txt"
    break
  fi
  if [[ "$(date +%s)" -ge "$END_AT" ]]; then break; fi

  sleep "$SAMPLE_SECONDS"
  elapsed=$((elapsed + SAMPLE_SECONDS))
done

for _ in $(seq 1 60); do
  kill -0 "$BOT_PID" 2>/dev/null || break
  sleep 2
done
kill "$BOT_PID" 2>/dev/null || true
wait "$BOT_PID" 2>/dev/null || true
cp -f "$ROOT/logs/errors.log" "$OUT/errors.log" 2>/dev/null || true

python3 - "$OUT" "$NAME" "$BOTS" "$MINUTES" "$PID" <<'PY'
import csv, json, os, statistics, sys

out, name, bots, minutes, pid = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5]
rows = list(csv.DictReader(open(os.path.join(out, "samples.csv"))))
report = {}
try:
    report = json.load(open(os.path.join(out, "bots.json")))
except Exception as exc:
    # Jendela soak bisa berakhir sebelum laporan ditulis; log bot tetap memuat
    # angka spawn terakhir, jadi gate tidak boleh gagal hanya karena file hilang.
    fallback = 0
    log_path = os.path.join(out, "bots.log")
    if os.path.exists(log_path):
        for line in open(log_path, errors="replace"):
            marker = "[SWARM] spawn "
            if marker in line:
                try:
                    fallback = int(line.split(marker, 1)[1].split("/", 1)[0])
                except (IndexError, ValueError):
                    pass
    report = {"error": f"laporan bot tidak terbaca: {exc}", "spawned": fallback}

def col(key):
    vals = []
    for row in rows:
        try:
            vals.append(float(row[key]))
        except (TypeError, ValueError):
            continue
    return vals

def q(vals, frac):
    if not vals:
        return 0.0
    ordered = sorted(vals)
    idx = min(len(ordered) - 1, int(round(frac * (len(ordered) - 1))))
    return ordered[idx]

rss = col("rss_mb")
perf_systems = ["perf_zombie", "perf_director", "perf_horde", "perf_player", "perf_hud"]
server_dead = any(line.startswith("SERVER MATI") for line in open(os.path.join(out, "summary.txt")).read().splitlines()) \
    if os.path.exists(os.path.join(out, "summary.txt")) else False
# RSS 0 berarti proses hilang; tanpa cek ini gate "server tetap hidup" lolos palsu
# saat server crash di tengah soak.
if any(float(row["rss_mb"]) == 0 for row in rows):
    server_dead = True
assertions = sum(col("assert"))
errlog = col("errlog")[-1] if rows else 0
spawned = report.get("spawned", 0)
worst_p95 = 0.0
lines = []
lines.append(f"# Soak {minutes} menit — instans {name}\n")
lines.append(f"- Bot diminta: **{bots}**, spawn: **{spawned}**, durasi sesi: {minutes} menit")
lines.append(f"- Sampel: {len(rows)} (interval {os.environ.get('SOAK_SAMPLE_SECONDS', 30)} dtk)")
lines.append(f"- RSS awal/akhir/puncak: {rss[0] if rss else 0} / {rss[-1] if rss else 0} / {max(rss) if rss else 0} MB")
lines.append(f"- Pertumbuhan RSS: {(rss[-1] - rss[0]) if len(rss) > 1 else 0} MB")
lines.append(f"- Zombie aktif maks: {int(max(col('zombie')) if col('zombie') else 0)}")
lines.append(f"- Assertion RakNet: {int(assertions)}; baris errors.log baru: {int(errlog)}")
for system in perf_systems:
    vals = col(system)
    lines.append(f"- tick {system.replace('perf_', '')}: p95 {q(vals, 0.95):.0f} ms, maks {max(vals) if vals else 0:.0f} ms")
    worst_p95 = max(worst_p95, q(vals, 0.95))

gates = [
    ("server tetap hidup", not server_dead),
    ("0 assertion RakNet / SIGABRT", int(assertions) == 0 and int(errlog) == 0),
    (f"spawn bot >= 90% ({spawned}/{bots})", bots > 0 and spawned >= 0.9 * bots),
    ("tick p95 semua sistem < 20 ms", worst_p95 < 20.0),
    ("pertumbuhan RSS < 15% RSS awal", (rss and (rss[-1] - rss[0]) <= 0.15 * max(rss[0], 1)) or False),
]
lines.append("\n## Gate\n")
lines.append("| Gate | Hasil |")
lines.append("|---|---|")
for label, ok in gates:
    lines.append(f"| {label} | {'PASS' if ok else 'FAIL'} |")
lines.append("\n**Verdict: " + ("LULUS" if all(ok for _, ok in gates) else "BELUM LULUS") + "**")

open(os.path.join(out, "summary.md"), "w").write("\n".join(lines) + "\n")
print("\n".join(lines))
PY

echo "== selesai. Ringkasan: $OUT/summary.md"
