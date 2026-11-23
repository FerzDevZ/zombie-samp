#!/usr/bin/env bash
# Laporan mingguan staff (ROADMAP: zona paling mematikan, senjata dominan).
#
#   ./scripts/weekly-report.sh           # 7 hari terakhir
#   ./scripts/weekly-report.sh 30        # 30 hari terakhir
#
# Hanya membaca (SELECT). Tidak menulis ke database.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTAINER="${MYSQL_CONTAINER:-laststand-mysql}"
USER_NAME="${MYSQL_USER:-laststand}"
PASS="${MYSQL_PASS:-change_me}"
DB="${MYSQL_DB:-laststand}"
DAYS="${1:-7}"

[[ "$DAYS" =~ ^[0-9]+$ ]] || { echo "GAGAL: jumlah hari harus angka." >&2; exit 1; }

if ! docker inspect -f '{{.State.Running}}' "$CONTAINER" 2>/dev/null | grep -q true; then
    echo "[LAPORAN] GAGAL: container '$CONTAINER' tidak jalan." >&2
    exit 1
fi

# kill_log.created_at disimpan dalam epoch milidetik (lihat core/mysql.inc).
SINCE_MS=$(( ($(date +%s) - DAYS * 86400) * 1000 ))

query() {
    docker exec "$CONTAINER" mariadb -u"$USER_NAME" -p"$PASS" -N -B "$DB" -e "$1"
}

section() {
    printf '\n%s\n%s\n' "$1" "$(printf '%.0s-' $(seq 1 ${#1}))"
}

echo "LAPORAN STAFF — LAST STAND"
echo "Periode: ${DAYS} hari terakhir (sejak $(date -d "@$(( SINCE_MS / 1000 ))" '+%Y-%m-%d %H:%M'))"
echo "Database: $DB"

section "Populasi"
query "SELECT CONCAT('karakter total: ', COUNT(*)) FROM characters;"
query "SELECT CONCAT('karakter aktif (main ${DAYS} hari): ', COUNT(*)) FROM characters
        WHERE last_seen_at >= $SINCE_MS;"
query "SELECT CONCAT('karakter dengan playtime > 0: ', COUNT(*)) FROM characters
        WHERE playtime_sec > 0;"
query "SELECT CONCAT('total playtime: ', ROUND(IFNULL(SUM(playtime_sec), 0) / 3600), ' jam') FROM characters;"

# kill_log hanya mencatat kill zombie (victim_kind = 2, lihat core/telemetry.inc).
# Kematian pemain tidak punya tabel; jangan tampilkan angka yang selalu 0.
section "Pertempuran (${DAYS} hari)"
query "SELECT CONCAT('zombie terbunuh: ', COUNT(*)) FROM kill_log
        WHERE victim_kind = 2 AND created_at >= $SINCE_MS;"

section "Zona paling mematikan (kill zombie per zona)"
query "SELECT CONCAT('zona ', zone_id, ': ', COUNT(*), ' kill') FROM kill_log
        WHERE victim_kind = 2 AND created_at >= $SINCE_MS AND zone_id IS NOT NULL
        GROUP BY zone_id ORDER BY COUNT(*) DESC LIMIT 5;" || true

section "Senjata dominan"
query "SELECT CONCAT('weapon ', weapon_id, ': ', COUNT(*), ' kill') FROM kill_log
        WHERE victim_kind = 2 AND created_at >= $SINCE_MS AND weapon_id IS NOT NULL
        GROUP BY weapon_id ORDER BY COUNT(*) DESC LIMIT 5;" || true

section "Top 5 pembunuh zombie"
query "SELECT CONCAT(c.name, ' — ', COUNT(*), ' kill') FROM kill_log k
        JOIN characters c ON c.id = k.killer_id
        WHERE k.victim_kind = 2 AND k.created_at >= $SINCE_MS
        GROUP BY k.killer_id ORDER BY COUNT(*) DESC LIMIT 5;" || true

section "Ekonomi"
query "SELECT CONCAT('uang beredar: \$', FORMAT(IFNULL(SUM(cash), 0), 0)) FROM characters;"
query "SELECT CONCAT('scrap beredar: ', FORMAT(IFNULL(SUM(scrap), 0), 0)) FROM characters;"

section "Moderasi"
query "SELECT CONCAT('ban aktif: ', COUNT(*)) FROM bans
        WHERE expires_at = 0 OR expires_at > UNIX_TIMESTAMP() * 1000;" || true
query "SELECT CONCAT('flag anticheat ${DAYS} hari: ', COUNT(*)) FROM anticheat_flags
        WHERE created_at >= $SINCE_MS;" || true

printf '\nSelesai. Sumber: tabel characters, kill_log, bans, anticheat_flags.\n'
