#!/usr/bin/env bash
# Nilai ulang folder soak memakai aturan gate TERBARU tanpa menjalankan ulang
# (dipakai setelah gate RSS diubah ke steady-state; data mentah tetap asli).
set -uo pipefail
DIR="${1:?pakai: scripts/soak_rescore.sh runtime/soak/<stempel>}"
python3 - "$DIR" <<'PY'
import csv, json, os, sys

out = sys.argv[1]
with open(os.path.join(out, 'samples.csv')) as handle:
    rows = list(csv.DictReader(handle))
with open(os.path.join(out, 'bots.json')) as handle:
    report = json.load(handle)

duration = float(rows[-1]['detik']) if rows else 0
rss = [float(r['rss_mb']) for r in rows]
zombie = [float(r['zombie']) for r in rows]
assertions = sum(int(r['assert']) for r in rows)
errlog = int(rows[-1]['errlog']) if rows else 0
spawned = report.get('spawned_ever', report.get('spawned', 0))
bots = report.get('count', spawned) or spawned
dead = any(value == 0 for value in rss)

steady = rss[0] if rss else 0.0
for row in rows:
    if float(row["detik"]) >= duration * 0.5:
        steady = float(row['rss_mb'])
        break

worst_p95 = 0.0
for key in [k for k in rows[0] if k.startswith('perf_')]:
    values = sorted(float(r[key]) for r in rows)
    worst_p95 = max(worst_p95, values[int(len(values) * 0.95) - 1] if values else 0)

zero_samples = sum(1 for index, value in enumerate(zombie) if value == 0 and float(rows[index]['detik']) >= 120)
gates = [
    ('server tetap hidup', not dead),
    ('0 assertion RakNet / SIGABRT', assertions == 0 and errlog == 0),
    (f'spawn bot >= 90% ({spawned}/{bots})', bots and spawned >= 0.9 * bots),
    ('tick p95 semua sistem < 20 ms', worst_p95 < 20.0),
    ('pertumbuhan RSS steady-state < 15%', steady > 0 and (rss[-1] - steady) <= 0.15 * steady and rss[-1] <= 1200),
    ('beban nyata: zombie pernah aktif', max(zombie) > 0 and zero_samples <= 3),
]
print(f"# Nilai ulang {out}\n")
print(f"- Durasi data: {duration:.0f} detik; RSS awal/steady/akhir: {rss[0]:.0f}/{steady:.0f}/{rss[-1]:.0f} MB")
print(f"- Zombie maks: {max(zombie):.0f}; spawn bot: {spawned}/{bots}; p95 terburuk: {worst_p95:.0f} ms\n")
failed = 0
for label, ok in gates:
    print(f"| {label} | {'PASS' if ok else 'FAIL'} |")
    failed += 0 if ok else 1
print(f"\n**Verdict: {'LULUS' if failed == 0 else 'BELUM LULUS'}**")
PY
