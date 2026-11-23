#!/usr/bin/env bash
# Cek kesehatan instance SA-MP lewat protokol query aslinya (UDP), bukan ping.
# Ping hanya membuktikan host hidup; ini membuktikan *gamemode* menjawab.
#
#   ./scripts/healthcheck.sh              # semua instance dari runtime/*/server.cfg
#   ./scripts/healthcheck.sh s1           # satu instance
#   ./scripts/healthcheck.sh --json       # keluaran mesin (cron/monitoring)
#
# Exit 0 = semua instance sehat, 1 = ada yang tidak menjawab.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JSON=0
TARGETS=()

for arg in "$@"; do
    case "$arg" in
        --json) JSON=1 ;;
        -h|--help) sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) TARGETS+=("$arg") ;;
    esac
done

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    for cfg in "$ROOT"/runtime/*/server.cfg; do
        [[ -f "$cfg" ]] || continue
        TARGETS+=("$(basename "$(dirname "$cfg")")")
    done
fi

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    echo "Tidak ada instance di $ROOT/runtime" >&2
    exit 1
fi

args=("${TARGETS[@]}")
if [[ $JSON -eq 1 ]]; then args+=(--json); fi

# Parser di Python: field SA-MP memakai panjang 4-byte sebelum tiap string,
# jadi memotong per NUL (cara cepat yang sering ditulis orang) menghasilkan
# hostname kosong. Sudah dibuktikan salah di probe pertama.
python3 - "$ROOT" "${args[@]}" <<'PY'
import json, socket, struct, sys
from pathlib import Path

root = Path(sys.argv[1])
raw = sys.argv[2:]
as_json = "--json" in raw
targets = [t for t in raw if t != "--json"]


def read_port(instance):
    cfg = root / "runtime" / instance / "server.cfg"
    for line in cfg.read_text(errors="replace").splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] == "port":
            return int(parts[1])
    return 0


def query(host, port, timeout=3.0):
    packet = b"SAMP" + socket.inet_aton(host) + struct.pack("<H", port) + b"i"
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    sock.sendto(packet, (host, port))
    data, _ = sock.recvfrom(2048)
    sock.close()
    if len(data) < 11 or data[:4] != b"SAMP":
        raise ValueError("bukan paket SA-MP")

    body = data[11:]
    # password(1) players(2) maxplayers(2) hostname(4+len) gamemode(4+len) language(4+len)
    passworded = body[0]
    players, maxplayers = struct.unpack("<HH", body[1:5])
    offset = 5
    texts = []
    for _ in range(3):
        (length,) = struct.unpack("<i", body[offset:offset + 4])
        offset += 4
        texts.append(body[offset:offset + length].decode("latin-1"))
        offset += length
    return {
        "passworded": bool(passworded),
        "players": players,
        "maxplayers": maxplayers,
        "hostname": texts[0].strip(),
        "gamemode": texts[1].strip(),
        "language": texts[2].strip(),
    }


results = []
failed = 0
for instance in targets:
    port = read_port(instance)
    entry = {"instance": instance, "port": port, "ok": False}
    if port <= 0:
        entry["error"] = "port tidak ada di server.cfg"
        failed += 1
        results.append(entry)
        continue

    last_error = None
    # Dua percobaan: satu paket UDP bisa hilang di jaringan yang sibuk.
    for _ in range(2):
        try:
            info = query("127.0.0.1", port)
            entry.update(info)
            entry["ok"] = True
            break
        except Exception as exc:
            last_error = f"{type(exc).__name__}: {exc}"
    if not entry["ok"]:
        entry["error"] = last_error or "tanpa jawaban"
        failed += 1
    results.append(entry)

if as_json:
    print(json.dumps(results, indent=2))
else:
    print(f"{'INSTANS':8} {'PORT':6} {'PEMAIN':9} {'MODE':10} HOSTNAME")
    for entry in results:
        if entry["ok"]:
            print(f"{entry['instance']:8} {entry['port']:<6} "
                  f"{str(entry['players']) + '/' + str(entry['maxplayers']):9} "
                  f"{entry['gamemode'][:9]:10} {entry['hostname'][:40]}")
        else:
            print(f"{entry['instance']:8} {entry['port']:<6} {'-':9} {'-':10} GAGAL: {entry['error']}")

sys.exit(1 if failed else 0)
PY
