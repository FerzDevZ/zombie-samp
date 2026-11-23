#!/usr/bin/env bash
# Terapkan seluruh migrasi database/migrations/*.sql secara berurutan ke satu
# database. Aman dijalankan berulang: setiap file memakai IF NOT EXISTS, dan
# versi yang sudah tercatat di schema_migrations dilewati.
#
#   ./scripts/db-migrate.sh laststand
#   ./scripts/db-migrate.sh laststand_s2
#
# Kredensial dibaca dari runtime/<instans>/mysql.ini supaya tidak ada password
# kedua yang harus dirawat.
set -euo pipefail

DB="${1:-}"
if [[ -z "$DB" ]]; then
    echo "Pemakaian: $0 <database>   (mis. laststand, laststand_s2)" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTAINER="${MYSQL_CONTAINER:-laststand-mysql}"
MIGRATIONS="$ROOT/database/migrations"

if [[ ! -d "$MIGRATIONS" ]]; then
    echo "Direktori migrasi tidak ada: $MIGRATIONS" >&2
    exit 1
fi

echo "[MIGRATE] target=$DB container=$CONTAINER"

docker exec -i "$CONTAINER" sh -c "mariadb -uroot -p\"\$MARIADB_ROOT_PASSWORD\" \"$DB\"" <<'SQL'
CREATE TABLE IF NOT EXISTS schema_migrations (
    version   VARCHAR(64) NOT NULL PRIMARY KEY,
    applied_at BIGINT UNSIGNED NOT NULL
);
SQL

applied=0
skipped=0

for file in "$MIGRATIONS"/*.sql; do
    name="$(basename "$file" .sql)"
    exists="$(docker exec -i "$CONTAINER" sh -c \
        "mariadb -uroot -p\"\$MARIADB_ROOT_PASSWORD\" -N -B \"$DB\" -e \"SELECT COUNT(*) FROM schema_migrations WHERE version='$name'\"" \
        2>/dev/null | tr -d '[:space:]')"

    if [[ "$exists" == "1" ]]; then
        skipped=$((skipped + 1))
        continue
    fi

    # Migrasi WAJIB atomik: tabel yang setengah jadi lebih buruk daripada gagal.
    if docker exec -i "$CONTAINER" sh -c \
        "mariadb -uroot -p\"\$MARIADB_ROOT_PASSWORD\" \"$DB\"" < "$file" 2>/tmp/db_migrate_err.txt; then
        docker exec -i "$CONTAINER" sh -c \
            "mariadb -uroot -p\"\$MARIADB_ROOT_PASSWORD\" \"$DB\" -e \"INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('$name', UNIX_TIMESTAMP())\"" \
            2>/dev/null
        echo "  OK   $name"
        applied=$((applied + 1))
    else
        echo "  GAGAL $name" >&2
        sed 's/^/        /' /tmp/db_migrate_err.txt | grep -v "Using a password" >&2
        exit 1
    fi
done

tables="$(docker exec -i "$CONTAINER" sh -c \
    "mariadb -uroot -p\"\$MARIADB_ROOT_PASSWORD\" -N -B \"$DB\" -e \"SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$DB'\"" \
    2>/dev/null | tr -d '[:space:]')"

echo "[MIGRATE] selesai: $applied diterapkan, $skipped dilewati, $tables tabel di $DB"
