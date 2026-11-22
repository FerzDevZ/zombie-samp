#!/usr/bin/env bash
# Backup database + config ke backups/<stamp>/ dengan checksum dan retensi.
# Pakai: ./scripts/backup.sh            (retensi default 7 backup terakhir)
#        BACKUP_KEEP=14 ./scripts/backup.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTAINER="${MYSQL_CONTAINER:-laststand-mysql}"
USER_NAME="${MYSQL_USER:-laststand}"
PASS="${MYSQL_PASS:-change_me}"
KEEP="${BACKUP_KEEP:-7}"
DBS=(laststand laststand_s2)

STAMP="$(date +%Y%m%d_%H%M%S)"
DEST="$ROOT/backups/$STAMP"
mkdir -p "$DEST"

if ! docker inspect -f '{{.State.Running}}' "$CONTAINER" 2>/dev/null | grep -q true; then
    echo "[BACKUP] GAGAL: container '$CONTAINER' tidak jalan." >&2
    exit 1
fi

for db in "${DBS[@]}"; do
    if ! docker exec "$CONTAINER" mariadb -u"$USER_NAME" -p"$PASS" -N -B \
        -e "SELECT 1" "$db" >/dev/null 2>&1; then
        echo "[BACKUP] GAGAL: database '$db' tidak bisa diakses." >&2
        exit 1
    fi

    docker exec "$CONTAINER" mariadb-dump -u"$USER_NAME" -p"$PASS" \
        --single-transaction --quick --routines --events \
        --default-character-set=utf8mb4 "$db" \
        | gzip -9 > "$DEST/$db.sql.gz"

    size="$(du -h "$DEST/$db.sql.gz" | cut -f1)"
    echo "[BACKUP] $db.sql.gz ($size)"
done

for f in server.cfg mysql.ini; do
    [ -f "$ROOT/$f" ] && cp "$ROOT/$f" "$DEST/root_$f"
done
# Kunci admin runtime ikut backup (tanpa ini restore kehilangan akses admin).
[ -f "$ROOT/scriptfiles/admin_keys.cfg" ] && cp "$ROOT/scriptfiles/admin_keys.cfg" "$DEST/root_admin_keys.cfg"
for inst in s1 s2; do
    for f in server.cfg mysql.ini; do
        [ -f "$ROOT/runtime/$inst/$f" ] && cp "$ROOT/runtime/$inst/$f" "$DEST/${inst}_$f"
    done
done

( cd "$DEST" && sha256sum ./*.gz ./*.cfg ./*.ini > SHA256SUMS 2>/dev/null || true )

{
    echo "stamp=$STAMP"
    echo "databases=${DBS[*]}"
    echo "host=$(hostname)"
    echo "amx_sha=$(sha256sum "$ROOT/gamemodes/main.amx" 2>/dev/null | cut -d' ' -f1)"
} > "$DEST/MANIFEST"

# Retensi: hapus backup tertua di luar KEEP terbaru.
count=0
while IFS= read -r old; do
    count=$((count + 1))
done < <(ls -1d "$ROOT/backups"/*/ 2>/dev/null | sort)

if [ "$count" -gt "$KEEP" ]; then
    removeTotal=$((count - KEEP))
    ls -1d "$ROOT/backups"/*/ | sort | head -n "$removeTotal" | while IFS= read -r old; do
        rm -rf "$old"
        echo "[BACKUP] retensi: hapus $(basename "$old")"
    done
fi

echo "[BACKUP] selesai -> $DEST"
