#!/usr/bin/env bash
# Gate kompilasi resmi: satu perintah, exit code 0 = AMX layak dijalankan.
# Pakai: ./scripts/build.sh [--clean]
#
# Butuh: sampctl (>= 1.16) dan pawncc 3.10.10 yang bisa dieksekusi di host ini.
# pawncc resmi adalah ELF 32-bit, jadi host wajib punya loader i386
# (libc6:i386). Bila tidak ada, skrip jatuh ke container i386 otomatis.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
AMX="$ROOT/gamemodes/main.amx"
LOG="${BUILD_LOG:-$ROOT/backups/build.log}"
mkdir -p "$(dirname "$LOG")"

fail() { printf '\033[31m[BUILD] GAGAL: %s\033[0m\n' "$1" >&2; exit 1; }

command -v sampctl >/dev/null 2>&1 || fail "sampctl tidak ada. Install: https://github.com/Southclaws/sampctl"
[[ -f "$ROOT/pawn.json" ]] || fail "pawn.json tidak ditemukan di $ROOT"

cd "$ROOT"

# Gate keamanan statis (skill samp-security-lint). Dijalankan SEBELUM kompilasi
# supaya build gagal lebih awal kalau ada format(..., params) atau mysql %s.
# Aturan yang hanya warning (poolsize-loop) tidak menggagalkan build.
if [[ -x "$ROOT/scripts/lint-security.sh" ]]; then
  echo "[BUILD] gate keamanan statis"
  set +e
  bash "$ROOT/scripts/lint-security.sh" >"$ROOT/backups/lint.log" 2>&1
  LINT_RC=$?
  set -e
  grep -v 'poolsize-loop' "$ROOT/backups/lint.log" | grep -v 'ringkasan' | sed '/^$/d' || true
  if [[ $LINT_RC -ne 0 ]]; then
    cat "$ROOT/backups/lint.log" >&2
    fail "gate keamanan statis menemukan error — lihat backups/lint.log"
  fi
fi

if [[ "${1:-}" == "--clean" ]]; then
  echo "[BUILD] bersihkan artefak lama"
  rm -f "$AMX" "$ROOT/gamemodes/main.amx.tmp" 2>/dev/null || true
fi

BEFORE_MTIME=0
[[ -f "$AMX" ]] && BEFORE_MTIME=$(stat -c %Y "$AMX")

if ldd /lib/ld-linux.so.2 >/dev/null 2>&1; then
  echo "[BUILD] compiler 32-bit tersedia, build native"
  set +e
  sampctl build >"$LOG" 2>&1
  RC=$?
  set -e
else
  echo "[BUILD] loader i386 tidak ada, build lewat container i386"
  PC_DIR="${SAMPSDK:-$HOME/samp-sdk}"
  PAWN_ROOT="${SAMPCTL_PAWN:-$HOME/.config/sampctl/pawn}"
  [[ -d "$PAWN_ROOT/pawn-lang/compiler/v3.10.10" ]] || fail "compiler v3.10.10 tidak ada di $PAWN_ROOT"
  set +e
  docker run --rm --platform linux/386 \
    -v "$ROOT:/srv" -v "$PAWN_ROOT:/pawn" -w /srv/gamemodes \
    -e LD_LIBRARY_PATH=/pawn/pawn-lang/compiler/v3.10.10 \
    i386/debian:bookworm-slim sh -c '
      set -e
      exec /pawn/pawn-lang/compiler/v3.10.10/pawncc main.pwn \
        -D/srv/gamemodes -o/srv/gamemodes/main.amx -d3 "-\;+" "-\(+" "-\+" "-Z+" \
        -i/srv -i/srv/dependencies/samp-stdlib -i/srv/dependencies/pawn-stdlib \
        -i/srv/dependencies/YSI-Includes -i/srv/dependencies/md-sort \
        -i/srv/dependencies/code-parse.inc -i/srv/dependencies/indirection \
        -i/srv/dependencies/amx_assembly -i/srv/dependencies/SA-MP-MySQL \
        -i/srv/dependencies/sscanf -i/srv/dependencies/samp-bcrypt/include \
        -i/srv/dependencies/FCNPC/sampsvr_files \
        -i/srv/dependencies/.resources/samp-streamer-plugin-7c00b9
    ' >"$LOG" 2>&1
  RC=$?
  set -e
  # AMX dibuat sebagai root di dalam container; kembalikan ke pemilik repo.
  [[ -f "$AMX" ]] && sudo -n chown "$(id -u):$(id -g)" "$AMX" 2>/dev/null || true
fi

WARN=$(grep -cE "warning [0-9]+" "$LOG" 2>/dev/null || true)
ERR=$(grep -cE "error [0-9]+" "$LOG" 2>/dev/null || true)
WARN=${WARN:-0}
ERR=${ERR:-0}

echo "[BUILD] log: $LOG"
if [[ "$RC" -ne 0 || "$ERR" -gt 0 ]]; then
  grep -E "error [0-9]+|warning [0-9]+" "$LOG" | tail -20 >&2 || true
  fail "$ERR error, $WARN warning (exit $RC)"
fi

[[ -f "$AMX" ]] || fail "kompilasi sukses tapi $AMX tidak terbentuk"
AFTER_MTIME=$(stat -c %Y "$AMX")
[[ "$AFTER_MTIME" -ge "$BEFORE_MTIME" ]] || fail "AMX tidak diperbarui (mtime lebih lama)"

# Gate tata letak HUD: TextDraw tidak punya perataan kanan, jadi baris yang
# meluber hanya ketahuan dari mata pemain. Skrip ini menolak build bila ada
# baris melewati 640 px atau tabel gHudFitRows tidak sinkron dengan HUD_Create.
if command -v python3 >/dev/null 2>&1; then
  if ! python3 "$ROOT/tools/hud_preview/generate.py" >/dev/null 2>>"$LOG"; then
    python3 "$ROOT/tools/hud_preview/generate.py" | grep -E "MELUBER|DRIFT|GATE" | sed 's/^/  /' >&2 || true
    fail "gate lebar HUD gagal (lihat tools/hud_preview/generate.py)"
  fi
  echo "[BUILD] gate HUD 640px: lulus"
  # Gate zona aman Android: posisi setiap HUD_X/HUD_Y/HUD_YT diperiksa di KETIGA
  # gaya HUD terhadap geometri klien 2340x1080 yang sudah diukur.
  if command -v python3 >/dev/null 2>&1; then
    if ! python3 "$ROOT/tools/hud_preview/android_zone.py" >>"$LOG" 2>&1; then
      python3 "$ROOT/tools/hud_preview/android_zone.py" | grep -E "ERROR|GAGAL" | head -8 | sed 's/^/  /' >&2 || true
      fail "gate zona aman Android gagal — elemen HUD di luar pita 320..446"
    fi
    echo "[BUILD] gate zona Android: lulus"

# Gate geometri panel bangun base: skala panel tidak boleh menumpuk tombol.
echo "[BUILD] gate panel bangun"
if ! python3 "$ROOT/tools/hud_preview/panel_layout.py" >>"$LOG" 2>&1; then
    python3 "$ROOT/tools/hud_preview/panel_layout.py" | grep -E "FAIL|GAGAL|->" | head -10 | sed 's/^/  /' >&2 || true
    fail "geometri panel bangun rusak (lihat backups/panel_layout.log)"
fi
grep -E "GATE PANEL" "$LOG" | tail -1 | sed 's/^/  /'
  fi
else
  echo "[BUILD] peringatan: python3 tidak ada, gate HUD dilewati"
fi

SIZE=$(stat -c %s "$AMX")
echo "[BUILD] OK — $(basename "$AMX") ${SIZE} byte, ${WARN} warning, 0 error"
[[ "$WARN" -gt 0 ]] && grep -E "warning [0-9]+" "$LOG" | sed 's/^/  /' | head -10
exit 0
