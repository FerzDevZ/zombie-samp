#!/usr/bin/env bash
# Restore database dari folder backup. Sebelum menimpa, kondisi saat ini
# otomatis di-backup dulu (backups/pre_restore_<stamp>).
# Pakai: ./scripts/restore.sh backups/20261118_150000
#        ./scripts/restore.sh            (pakai backup terbaru)
#        ./scripts/restore.sh <dir> --yes (tanpa konfirmasi)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTAINER="${MYSQL_CONTAINER:-laststand-mysql}"
USER_NAME="${MYSQL_USER:-laststand}"
PASS="${MYSQL_PASS:-change_me}"

SRC="${1:-}"
ASSUME_YES=0
if [ "${2:-}" = "--yes" ] || [ "${1:-}" = "--yes" ]; then
    ASSUME_YES=1
    [ "${1:-}" = "--yes" ] && SRC=""
fi

if [ -z "$SRC" ]; then
    SRC="$(ls -1d "$ROOT/backups"/*/ 2>/dev/null | sort | tail -1)"
    if [ -z "$SRC" ]; then
        echo "[RESTORE] GAGAL: tidak ada folder backup di $ROOT/backups." >&2
        exit 1
    fi
fi

SRC="${SRC%/}"
if [ ! -d "$SRC" ]; then
    echo "[RESTORE] GAGAL: folder '$SRC' tidak ditemukan." >&2
    exit 1
fi

mapfile -t dumps < <(ls -1 "$SRC"/*.sql.gz 2>/dev/null)
if [ "${#dumps[@]}" -eq 0 ]; then
    echo "[RESTORE] GAGAL: tidak ada file .sql.gz di '$SRC'." >&2
    exit 1
fi

if [ -f "$SRC/SHA256SUMS" ]; then
    ( cd "$SRC" && sha256sum -c SHA256SUMS >/dev/null 2>&1 ) \
        && echo "[RESTORE] checksum OK" \
        || { echo "[RESTORE] GAGAL: checksum tidak cocok, backup rusak." >&2; exit 1; }
fi

if ! docker inspect -f '{{.State.Running}}' "$CONTAINER" 2>/dev/null | grep -q true; then
    echo "[RESTORE] GAGAL: container '$CONTAINER' tidak jalan." >&2
    exit 1
fi

echo "[RESTORE] sumber: $SRC"
for dump in "${dumps[@]}"; do
    echo "  - $(basename "$dump")"
done

if [ "$ASSUME_YES" -ne 1 ]; then
    printf "[RESTORE] Data live AKAN DITIMPA. Ketik RESTORE untuk lanjut: "
    read -r jawab
    if [ "$jawab" != "RESTORE" ]; then
        echo "[RESTORE] dibatalkan."
        exit 1
    fi
fi

# Jaring aman: snapshot kondisi sekarang dulu.
SAFE="$ROOT/backups/pre_restore_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$SAFE"
for dump in "${dumps[@]}"; do
    db="$(basename "$dump" .sql.gz)"
    docker exec "$CONTAINER" mariadb-dump -u"$USER_NAME" -p"$PASS" \
        --single-transaction --quick --routines --default-character-set=utf8mb4 "$db" \
        | gzip -9 > "$SAFE/$db.sql.gz"
done
echo "[RESTORE] snapshot pra-restore: $SAFE"

for dump in "${dumps[@]}"; do
    db="$(basename "$dump" .sql.gz)"
    echo "[RESTORE] import $db ..."
    docker exec "$CONTAINER" mariadb -u"$USER_NAME" -p"$PASS" -e "CREATE DATABASE IF NOT EXISTS \`$db\`"
    gunzip -c "$dump" | docker exec -i "$CONTAINER" mariadb -u"$USER_NAME" -p"$PASS" "$db"

    tables="$(docker exec "$CONTAINER" mariadb -u"$USER_NAME" -p"$PASS" -N -B \
        -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$db'")"
    echo "[RESTORE] $db selesai, $tables tabel"
done

# Config runtime ikut dipulihkan (tanpa ini restore kehilangan kunci admin &
# kredensial DB — celah yang ditemukan drill 26-11-19). File root_* dari backup.
if [ -f "$SRC/root_admin_keys.cfg" ]; then
    cp "$SRC/root_admin_keys.cfg" "$ROOT/scriptfiles/admin_keys.cfg"
    chmod 600 "$ROOT/scriptfiles/admin_keys.cfg"
    echo "[RESTORE] scriptfiles/admin_keys.cfg dipulihkan"
fi
if [ -f "$SRC/root_mysql.ini" ]; then
    cp "$SRC/root_mysql.ini" "$ROOT/mysql.ini"
    echo "[RESTORE] mysql.ini dipulihkan"
fi
if [ -f "$SRC/root_server.cfg" ]; then
    cp "$SRC/root_server.cfg" "$ROOT/server.cfg"
    echo "[RESTORE] server.cfg dipulihkan"
fi
for inst in s1 s2; do
    for f in server.cfg mysql.ini; do
        if [ -f "$SRC/${inst}_$f" ] && [ -d "$ROOT/runtime/$inst" ]; then
            cp "$SRC/${inst}_$f" "$ROOT/runtime/$inst/$f"
            echo "[RESTORE] runtime/$inst/$f dipulihkan"
        fi
    done
done

echo "[RESTORE] selesai. Restart server agar data dimuat ulang: ./scripts/instance.sh restart s1"
