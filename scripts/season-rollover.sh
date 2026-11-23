#!/usr/bin/env bash
# Arsip leaderboard musiman (ROADMAP 233).
#
#   ./scripts/season-rollover.sh              # arsipkan bulan lalu
#   ./scripts/season-rollover.sh 2026-11      # arsipkan bulan tertentu
#
# Idempoten: PRIMARY KEY (season, category, rank_no) + ON DUPLICATE KEY UPDATE,
# jadi menjalankannya dua kali tidak menggandakan baris.
set -euo pipefail

CONTAINER="${MYSQL_CONTAINER:-laststand-mysql}"
USER_NAME="${MYSQL_USER:-laststand}"
PASS="${MYSQL_PASS:-change_me}"
DB="${MYSQL_DB:-laststand}"
TOP_N=10

SEASON="${1:-$(date -d "$(date +%Y-%m-01) -1 day" +%Y-%m)}"
[[ "$SEASON" =~ ^[0-9]{4}-(0[1-9]|1[0-2])$ ]] || {
    echo "GAGAL: format musim harus YYYY-MM (contoh 2026-11)." >&2
    exit 1
}

if ! docker inspect -f '{{.State.Running}}' "$CONTAINER" 2>/dev/null | grep -q true; then
    echo "[MUSIM] GAGAL: container '$CONTAINER' tidak jalan." >&2
    exit 1
fi

# Batas epoch milidetik untuk bulan tersebut (kill_log.created_at = epoch ms).
MONTH_START_MS=$(date -d "$SEASON-01 00:00:00" +%s)000
MONTH_END_MS=$(date -d "$SEASON-01 00:00:00 +1 month" +%s)000
NOW=$(date +%s)

sql() {
    docker exec "$CONTAINER" mariadb -u"$USER_NAME" -p"$PASS" -N -B "$DB" -e "$1"
}

echo "[MUSIM] Mengarsipkan $SEASON (batas: $MONTH_START_MS .. $MONTH_END_MS)"

sql "INSERT INTO season_archive (season, category, rank_no, char_id, char_name, score, archived_at)
     SELECT '$SEASON', 'kill', ranked.rn, ranked.char_id, ranked.char_name, ranked.score, $NOW
     FROM (
       SELECT ROW_NUMBER() OVER (ORDER BY COUNT(*) DESC) AS rn,
              k.killer_id AS char_id, c.name AS char_name, COUNT(*) AS score
       FROM kill_log k JOIN characters c ON c.id = k.killer_id
       WHERE k.victim_kind = 2 AND k.created_at >= $MONTH_START_MS AND k.created_at < $MONTH_END_MS
       GROUP BY k.killer_id, c.name
       ORDER BY score DESC LIMIT $TOP_N
     ) AS ranked
     ON DUPLICATE KEY UPDATE char_id = VALUES(char_id), char_name = VALUES(char_name),
       score = VALUES(score), archived_at = VALUES(archived_at);"

# Kategori di bawah ini adalah SNAPSHOT nilai saat arsip dibuat, bukan selisih
# bulanan: playtime_sec/cash/rank_points tidak punya riwayat per bulan.
for pair in "uang:cash" "jam:playtime_sec" "rank:rank_points"; do
    category="${pair%%:*}"
    column="${pair##*:}"
    sql "INSERT INTO season_archive (season, category, rank_no, char_id, char_name, score, archived_at)
         SELECT '$SEASON', '$category', ranked.rn, ranked.id, ranked.name, ranked.score, $NOW
         FROM (
           SELECT ROW_NUMBER() OVER (ORDER BY $column DESC) AS rn, id, name, $column AS score
           FROM characters WHERE $column > 0
           ORDER BY score DESC LIMIT $TOP_N
         ) AS ranked
         ON DUPLICATE KEY UPDATE char_id = VALUES(char_id), char_name = VALUES(char_name),
           score = VALUES(score), archived_at = VALUES(archived_at);"
done

echo "[MUSIM] Hasil:"
sql "SELECT CONCAT('  ', category, ': ', COUNT(*), ' baris') FROM season_archive
     WHERE season = '$SEASON' GROUP BY category ORDER BY category;"
