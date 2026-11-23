#!/usr/bin/env bash
# Pasang jadwal harian/mingguan (ROADMAP: cron/systemd timer).
#
#   ./scripts/install-schedule.sh            # cetak jadwal (default, tidak mengubah apa pun)
#   ./scripts/install-schedule.sh --install  # tambahkan ke crontab pengguna ini
#   ./scripts/install-schedule.sh --remove   # hapus baris yang dipasang skrip ini
#
# Tidak butuh root: memakai crontab milik pengguna. Backup harian memanggil
# scripts/backup.sh (butuh container MariaDB jalan).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MARK_BEGIN="# >>> zombie-samp schedule >>>"
MARK_END="# <<< zombie-samp schedule <<<"

BACKUP_AT="${BACKUP_AT:-15 4 * * *}"      # 04:15 setiap hari
REPORT_AT="${REPORT_AT:-20 4 * * 1}"      # Senin 04:20, setelah backup

render() {
  cat <<EOF
$MARK_BEGIN
$BACKUP_AT cd $ROOT && ./scripts/backup.sh >> backups/cron-backup.log 2>&1
$REPORT_AT cd $ROOT && ./scripts/weekly-report.sh >> backups/cron-report.log 2>&1
$MARK_END
EOF
}

case "${1:-}" in
  --print|"")
    render
    ;;
  --install)
    command -v crontab >/dev/null 2>&1 || { echo "GAGAL: crontab tidak terpasang." >&2; exit 1; }
    [[ -x "$ROOT/scripts/backup.sh" ]] || { echo "GAGAL: scripts/backup.sh tidak executable." >&2; exit 1; }
    mkdir -p "$ROOT/backups"
    current="$(crontab -l 2>/dev/null || true)"
    if printf '%s\n' "$current" | grep -qF "$MARK_BEGIN"; then
      echo "Sudah terpasang. Jalankan --remove dulu untuk memasang ulang."
      exit 0
    fi
    { printf '%s\n' "$current"; render; } | crontab -
    echo "Terpasang. Lihat dengan: crontab -l"
    ;;
  --remove)
    current="$(crontab -l 2>/dev/null || true)"
    printf '%s\n' "$current" | sed "/$MARK_BEGIN/,/$MARK_END/d" | crontab -
    echo "Baris zombie-samp dihapus dari crontab."
    ;;
  *)
    echo "Pemakaian: $0 [--print|--install|--remove]" >&2
    exit 1
    ;;
esac
