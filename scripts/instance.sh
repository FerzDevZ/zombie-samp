#!/usr/bin/env bash
# Manager multi-instans: satu VPS, banyak server LAST STAND, DB terpisah.
# Pakai: ./scripts/instance.sh <init|start|stop|restart|status|logs|list|db> [nama] [opsi]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUNTIME="$ROOT/runtime"
MYSQL_CONTAINER="${MYSQL_CONTAINER:-laststand-mysql}"
MYSQL_USER="${MYSQL_USER:-laststand}"
MYSQL_PASS="${MYSQL_PASS:-change_me}"
MYSQL_ROOT_PASS="${MYSQL_ROOT_PASS:-change_root_me}"

usage() {
  cat <<'TXT'
Perintah:
  init <nama> <port> <database> [hostname]   buat instans (dir + DB + konfigurasi)
  db <nama>                                  terapkan migrasi ke DB instans (jika kosong)
  prep <nama>                                siapkan dir (symlink bersama + kredensial) untuk supervisor
  start|stop|restart|status|logs <nama|all>
  list                                       tampilkan semua instans
TXT
}

instance_dir() { echo "$RUNTIME/$1"; }

require_instance() {
  local name="$1"
  [[ -n "$name" ]] || { echo "Nama instans wajib diisi (contoh: s1)." >&2; exit 1; }
  [[ -d "$(instance_dir "$name")" ]] || { echo "Instans '$name' belum ada. Jalankan init dulu." >&2; exit 1; }
}

write_server_cfg() {
  local dir="$1" port="$2" hostname="$3" rcon="$4"

  cat > "$dir/server.cfg" <<CFG
gamemode0 main
plugins mysql.so sscanf.so ColAndreas.so streamer.so samp_bcrypt.so FCNPC.so
rcon_password $rcon
port $port
hostname $hostname
maxplayers 400
language -
mapname San Andreas
weburl www.sa-mp.com
gamemodetext LAST STAND 0.1
announce 0
lanmode 0
query 1
rcon 0
logqueries 0
sleep 5
maxnpc 200
stream_rate 1000
stream_distance 300.000000
onfoot_rate 40
incar_rate 40
weapon_rate 40
chatlogging 1
timestamp 1
CFG
}

link_shared() {
  local dir="$1"
  for shared in samp03svr samp-npc announce npcmodes plugins gamemodes filterscripts; do
    [[ -e "$ROOT/$shared" ]] || continue
    ln -sfn "$ROOT/$shared" "$dir/$shared"
  done
  mkdir -p "$dir/scriptfiles/logs"
  # Data cadb 52 MB dipakai bersama: collision map tidak perlu diduplikasi per instans.
  ln -sfn "$ROOT/scriptfiles/colandreas" "$dir/scriptfiles/colandreas"
  ln -sfn "$ROOT/scriptfiles/ColAndreas" "$dir/scriptfiles/ColAndreas"
  [[ -d "$ROOT/scriptfiles/nodes" ]] && ln -sfn "$ROOT/scriptfiles/nodes" "$dir/scriptfiles/nodes"
  # Kunci admin runtime dipakai bersama semua instans (rotasi satu file).
  [[ -f "$ROOT/scriptfiles/admin_keys.cfg" ]] && ln -sfn "$ROOT/scriptfiles/admin_keys.cfg" "$dir/scriptfiles/admin_keys.cfg"
}

cmd_init() {
  local name="${1:-}" port="${2:-}" db="${3:-}" hostname="${4:-LAST STAND $name}"
  if [[ "$hostname" == *"#"* ]]; then
    echo "PERINGATAN: '#' memulai komentar di server.cfg, nama server akan terpotong." >&2
  fi
  [[ -n "$name" && -n "$port" && -n "$db" ]] || { usage; exit 1; }

  local dir; dir="$(instance_dir "$name")"
  mkdir -p "$dir"
  link_shared "$dir"
  write_server_cfg "$dir" "$port" "$hostname" "${name}-rcon-change-me"

  cat > "$dir/mysql.ini" <<INI
hostname = 127.0.0.1
username = $MYSQL_USER
password = $MYSQL_PASS
database = $db
pool_size = 4
auto_reconnect = true
INI

  # Tx_Boot membaca kredensial dari scriptfiles (sandbox fopen PAWN).
  mkdir -p "$dir/scriptfiles"
  cp -f "$dir/mysql.ini" "$dir/scriptfiles/mysql.ini"

  echo "Instans '$name' siap: $dir (port $port, DB $db)"
  cmd_db "$name"
}

cmd_db() {
  local name="${1:-}"; require_instance "$name"
  local dir; dir="$(instance_dir "$name")"
  local db; db="$(awk -F'= *' '/^database/{print $2}' "$dir/mysql.ini")"

  # User aplikasi tidak boleh punya izin CREATE DATABASE: pakai root hanya untuk ini.
  if ! docker exec "$MYSQL_CONTAINER" mariadb -uroot -p"$MYSQL_ROOT_PASS" -e \
      "CREATE DATABASE IF NOT EXISTS \`$db\`; GRANT ALL PRIVILEGES ON \`$db\`.* TO '$MYSQL_USER'@'%'; FLUSH PRIVILEGES;" >/dev/null 2>&1; then
    echo "GAGAL membuat DB '$db'. Cek MYSQL_ROOT_PASS atau container $MYSQL_CONTAINER." >&2
    return 1
  fi

  local tables
  tables="$(docker exec "$MYSQL_CONTAINER" mariadb -u"$MYSQL_USER" -p"$MYSQL_PASS" -N -B "$db" \
    -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$db'" 2>/dev/null)"

  if [[ "$tables" != "0" ]]; then
    echo "DB '$db' sudah berisi $tables tabel, migrasi dilewati."
    return 0
  fi

  for migration in "$ROOT"/database/migrations/*.sql; do
    echo "  migrasi $(basename "$migration")"
    if ! docker exec -i "$MYSQL_CONTAINER" mariadb -u"$MYSQL_USER" -p"$MYSQL_PASS" "$db" < "$migration" 2>/dev/null; then
      echo "GAGAL menerapkan $(basename "$migration") ke '$db'." >&2
      return 1
    fi
  done
  echo "DB '$db' siap."
}

# Dipakai ExecStartPre systemd: menyiapkan direktori TANPA menyalakan proses,
# karena samp03svr harus berjalan di foreground agar bisa diawasi systemd
# (cron seccomp membunuh proses 32-bit dengan SIGSYS saat restart).
cmd_prep() {
  local name="${1:-}"; require_instance "$name"
  local dir; dir="$(instance_dir "$name")"
  link_shared "$dir"
  mkdir -p "$dir/scriptfiles/logs"
  cp -f "$dir/mysql.ini" "$dir/scriptfiles/mysql.ini"
  echo "Instans '$name' siap dijalankan supervisor (dir $dir)."
}

cmd_start() {
  local name="${1:-}"; require_instance "$name"
  local dir; dir="$(instance_dir "$name")"

  if [[ -f "$dir/server.pid" ]] && kill -0 "$(cat "$dir/server.pid")" 2>/dev/null; then
    echo "Instans '$name' sudah jalan (pid $(cat "$dir/server.pid"))."; return 0
  fi

  # Tautan bersama (cadb, kunci admin) dibuat ulang tiap start: instans lama
  # tidak punya symlink admin_keys.cfg -> login kunci mati tanpa jejak.
  link_shared "$dir"

  pushd "$dir" >/dev/null
  # Tx_Boot membaca kredensial dari scriptfiles (sandbox fopen PAWN).
  mkdir -p scriptfiles
  cp -f mysql.ini scriptfiles/mysql.ini
  # samp03svr 32-bit + plugin ber-thread (FCNPC, MySQL) membuat glibc membuka
  # banyak arena; tiap arena menyimpan chunk bebas sendiri sehingga RSS naik
  # walau heap berguna kecil. Batasi arena supaya RSS tidak menggelembung.
  MALLOC_ARENA_MAX="${MALLOC_ARENA_MAX:-2}" \
  LD_LIBRARY_PATH="$dir/plugins:$ROOT${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    nohup ./samp03svr > console.log 2>&1 &
  local pid=$!
  popd >/dev/null

  echo "$pid" > "$dir/server.pid"
  sleep 2
  echo "Instans '$name' start (pid $pid), log: $dir/server_log.txt"
}

cmd_stop() {
  local name="${1:-}"; require_instance "$name"
  local dir; dir="$(instance_dir "$name")"

  if [[ ! -f "$dir/server.pid" ]]; then echo "Instans '$name' tidak punya pid file."; return 0; fi
  local pid; pid="$(cat "$dir/server.pid")"

  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    for _ in $(seq 1 15); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
    kill -9 "$pid" 2>/dev/null || true
  fi

  rm -f "$dir/server.pid"
  echo "Instans '$name' stop."
}

cmd_status() {
  local name="${1:-}"; require_instance "$name"
  local dir; dir="$(instance_dir "$name")"
  local port; port="$(awk '/^port /{print $2}' "$dir/server.cfg")"
  local db; db="$(awk -F'= *' '/^database/{print $2}' "$dir/mysql.ini")"

  if [[ -f "$dir/server.pid" ]] && kill -0 "$(cat "$dir/server.pid")" 2>/dev/null; then
    local rss; rss="$(ps -o rss= -p "$(cat "$dir/server.pid")" | awk '{printf "%.0f MB", $1/1024}')"
    printf '%-6s %-6s %-24s pid %-7s %s\n' "$name" "$port" "$db" "$(cat "$dir/server.pid")" "$rss"
  else
    printf '%-6s %-6s %-24s %s\n' "$name" "$port" "$db" "STOPPED"
  fi
}

cmd_logs() {
  local name="${1:-}"; require_instance "$name"
  local dir; dir="$(instance_dir "$name")"
  echo "== server_log =="; tail -n 25 "$dir/server_log.txt" 2>/dev/null || true
  echo "== errors =="; tail -n 10 "$dir/logs/errors.log" 2>/dev/null || true
}

cmd_list() {
  [[ -d "$RUNTIME" ]] || { echo "Belum ada instans."; return 0; }
  printf '%-6s %-6s %-24s %s\n' NAMA PORT DB STATUS
  for dir in "$RUNTIME"/*/; do
    [[ -f "$dir/mysql.ini" ]] || continue
    cmd_status "$(basename "$dir")"
  done
}

run_all() {
  local action="$1"
  for dir in "$RUNTIME"/*/; do
    [[ -f "$dir/mysql.ini" ]] || continue
    "$0" "$action" "$(basename "$dir")"
  done
}

case "${1:-}" in
  init)    shift; cmd_init "$@" ;;
  prep)    shift; cmd_prep "$@" ;;
  db)      shift; cmd_db "$@" ;;
  start)   [[ "${2:-}" == "all" ]] && run_all start || { shift; cmd_start "$@"; } ;;
  stop)    [[ "${2:-}" == "all" ]] && run_all stop || { shift; cmd_stop "$@"; } ;;
  restart) [[ "${2:-}" == "all" ]] && run_all restart || { shift; cmd_stop "$@"; cmd_start "$@"; } ;;
  status)  [[ "${2:-}" == "all" ]] && cmd_list || { shift; cmd_status "$@"; } ;;
  logs)    shift; cmd_logs "$@" ;;
  list)    cmd_list ;;
  *)       usage; exit 1 ;;
esac
