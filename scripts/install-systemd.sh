#!/usr/bin/env bash
# Pasang unit systemd untuk semua instance LAST STAND (s1, s2, ...).
# Idempoten: aman dijalankan ulang setelah menambah instance atau mengubah unit.
#
#   ./scripts/install-systemd.sh            # pasang + enable + start
#   ./scripts/install-systemd.sh --dry-run  # tampilkan rencana saja
#
# Kenapa systemd, bukan cron: cron di mesin ini memakai sandbox seccomp yang
# membunuh samp03svr 32-bit dengan SIGSYS (terbukti "Bad system call" di
# backups/watchdog.log). systemd tidak memakai sandbox itu.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_DIR="${UNIT_DIR:-/etc/systemd/system}"
DRY=0
[[ "${1:-}" == "--dry-run" ]] && DRY=1

if [[ $EUID -ne 0 ]]; then
    echo "Jalankan dengan sudo: sudo $0" >&2
    exit 1
fi

mapfile -t INSTANCES < <(for dir in "$ROOT"/runtime/*/server.cfg; do basename "$(dirname "$dir")"; done)
if [[ ${#INSTANCES[@]} -eq 0 ]]; then
    echo "Tidak ada instans di $ROOT/runtime" >&2
    exit 1
fi

echo "[SYSTEMD] instans terdeteksi: ${INSTANCES[*]}"

# Proses manual (instance.sh start) memegang port yang sama; hentikan agar
# systemd tidak gagal bind 7777/7778.
for instance in "${INSTANCES[@]}"; do
    if [[ -f "$ROOT/runtime/$instance/server.pid" ]] && kill -0 "$(cat "$ROOT/runtime/$instance/server.pid")" 2>/dev/null; then
        if [[ $DRY -eq 1 ]]; then
            echo "[DRY] akan menghentikan proses manual $instance"
        else
            echo "[SYSTEMD] menghentikan proses manual $instance"
            "$ROOT/scripts/instance.sh" stop "$instance" >/dev/null || true
        fi
    fi
done

for instance in "${INSTANCES[@]}"; do
    unit="$ROOT/scripts/systemd/laststand-$instance.service"
    if [[ ! -f "$unit" ]]; then
        echo "[SYSTEMD] unit untuk $instance belum ada ($unit). Jalankan dulu pembuatannya." >&2
        exit 1
    fi
    if [[ $DRY -eq 1 ]]; then
        echo "[DRY] pasang laststand-$instance.service -> $UNIT_DIR"
    else
        install -m 0644 "$unit" "$UNIT_DIR/laststand-$instance.service"
        echo "[SYSTEMD] terpasang $UNIT_DIR/laststand-$instance.service"
    fi
done

if [[ $DRY -eq 1 ]]; then
    echo "[DRY] pasang laststand.target -> $UNIT_DIR, daemon-reload, enable --now"
    exit 0
fi

install -m 0644 "$ROOT/scripts/systemd/laststand.target" "$UNIT_DIR/laststand.target"
systemctl daemon-reload
systemctl enable laststand.target >/dev/null

for instance in "${INSTANCES[@]}"; do
    systemctl enable "laststand-$instance.service" >/dev/null
done

systemctl restart laststand.target

sleep 3
for instance in "${INSTANCES[@]}"; do
    state="$(systemctl is-active "laststand-$instance.service" 2>/dev/null || true)"
    echo "[SYSTEMD] laststand-$instance.service = $state"
done

echo "[SYSTEMD] selesai. Cek kesehatan: $ROOT/scripts/healthcheck.sh"
