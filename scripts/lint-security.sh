#!/usr/bin/env bash
# Gate keamanan statis Pawn — diadaptasi dari skill `samp-security-lint`.
#
# Setiap aturan punya corpus dua arah di dalam skrip ini sendiri: sampel buruk
# (harus tertangkap) dan sampel bersih (tidak boleh tertangkap). Kalau sebuah
# aturan mulai menandai kode aman, self-test di bawah gagal dan gate ini tidak
# bisa dipakai. Tanpa itu, lint hanya jadi teks nasihat.
#
# Pakai:
#   ./scripts/lint-security.sh              # scan gamemodes/, gagal pada error
#   ./scripts/lint-security.sh --warn-only  # laporkan, jangan gagalkan build
#   ./scripts/lint-security.sh --self-test  # uji aturan itu sendiri
#
# Exit: 0 bersih, 1 ada temuan level error.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${TARGET:-$ROOT/gamemodes}"
WARN_ONLY=0
SELF_TEST=0
for arg in "$@"; do
  case "$arg" in
    --warn-only) WARN_ONLY=1 ;;
    --self-test) SELF_TEST=1 ;;
  esac
done

ERRORS=0
WARNINGS=0
declare -a FINDINGS=()

add_error() { ERRORS=$((ERRORS + 1)); FINDINGS+=("ERROR|$1|$2"); }
add_warn()  { WARNINGS=$((WARNINGS + 1)); FINDINGS+=("WARN|$1|$2"); }

# ---------------------------------------------------------------------------
# Aturan. Semua memakai grep -E dengan path:line agar bisa diklik di editor.
# ---------------------------------------------------------------------------

# 1) format-injection — input pemain dipakai sebagai FORMAT string.
#    `format(buf, size, "%s", params)` aman dan tidak tertangkap.
rule_format_injection() {
  local hits
  hits=$(grep -rnE 'format[[:space:]]*\([^,]*,[^,]*,[[:space:]]*(params|inputtext|cmdtext|dialogText)[[:space:]]*[,)]' \
    --include=*.pwn --include=*.inc "$1" 2>/dev/null)
  [[ -n "$hits" ]] && while IFS= read -r line; do add_error "format-injection" "$line"; done <<< "$hits"
  return 0
}

# 2) mysql-percent-s — %s di mysql_format berarti interpolasi tanpa escape.
rule_mysql_percent_s() {
  local hits
  hits=$(grep -rnE 'mysql_format[[:space:]]*\(.*"[^"]*%s' --include=*.pwn --include=*.inc "$1" 2>/dev/null)
  [[ -n "$hits" ]] && while IFS= read -r line; do add_error "mysql-percent-s" "$line"; done <<< "$hits"
  return 0
}

# 3) dialog-unguarded — OnDialogResponse yang memakai listitem tanpa menyentuh
#    dialogid sama sekali. Router yang meneruskan dialogid ke handler modul
#    TIDAK dianggap pelanggaran: guard-nya ada di pemilik dialog masing-masing.
rule_dialog_unguarded() {
  local report
  report=$(python3 - "$1" <<'PY'
import sys, pathlib, re
root = pathlib.Path(sys.argv[1])
hits = []
for path in list(root.rglob('*.inc')) + list(root.rglob('*.pwn')):
    text = path.read_text(encoding='utf-8', errors='replace').splitlines()
    inside = False
    start = 0
    depth = 0
    body = []
    for idx, line in enumerate(text, 1):
        if not inside and re.search(r'public\s+OnDialogResponse\s*\(', line):
            inside = True
            start = idx
            depth = 0
            body = []
            depth += line.count('{') - line.count('}')
            continue
        if inside:
            depth += line.count('{') - line.count('}')
            body.append((idx, line))
            if depth <= 0:
                inside = False
                joined = '\n'.join(l for _, l in body)
                if 'listitem' in joined and 'dialogid' not in joined:
                    hits.append(f'{path}:{start}: OnDialogResponse memakai listitem tanpa guard dialogid')
    if inside and body:
        joined = '\n'.join(l for _, l in body)
        if 'listitem' in joined and 'dialogid' not in joined:
            hits.append(f'{path}:{start}: OnDialogResponse memakai listitem tanpa guard dialogid')
print('\n'.join(hits))
PY
)
  [[ -n "$report" ]] && while IFS= read -r line; do add_error "dialog-unguarded" "$line"; done <<< "$report"
  return 0
}

# 4) poolsize-loop — GetPlayerPoolSize()/GetVehiclePoolSize() sebagai batas loop.
#    open.mp menandainya deprecated; di FCNPC id tinggi memang perlu, jadi warn.
rule_poolsize_loop() {
  local hits
  hits=$(grep -rnE '(GetPlayerPoolSize|GetVehiclePoolSize)[[:space:]]*\(' \
    --include=*.pwn --include=*.inc "$1" 2>/dev/null | grep -vE '^\S+:[0-9]+:[[:space:]]*(/\*|\*|//|[-+])')
  [[ -n "$hits" ]] && while IFS= read -r line; do add_warn "poolsize-loop" "$line"; done <<< "$hits"
  return 0
}

# 5) weak-rcon — password RCON pendek atau default yang diketahui.
rule_weak_rcon() {
  local cfg
  for cfg in "$ROOT"/runtime/*/server.cfg; do
    [[ -f "$cfg" ]] || continue
    local pass
    pass=$(grep -E '^[[:space:]]*rcon_password' "$cfg" | head -1 | sed 's/.*[[:space:]]//')
    if [[ -z "$pass" || ${#pass} -lt 8 || "$pass" == "changeme" || "$pass" == "rcon" ]]; then
      add_error "weak-rcon" "$cfg: rcon_password terlalu lemah (${#pass} karakter)"
    fi
  done
  return 0
}

run_rules() {
  rule_format_injection "$1"
  rule_mysql_percent_s "$1"
  rule_dialog_unguarded "$1"
  rule_poolsize_loop "$1"
  rule_weak_rcon
}

# ---------------------------------------------------------------------------
# Self-test: aturan harus menangkap sampel buruk DAN membiarkan sampel aman.
# ---------------------------------------------------------------------------
if [[ $SELF_TEST -eq 1 ]]; then
  TMP=$(mktemp -d)
  mkdir -p "$TMP/bad" "$TMP/good"

  cat > "$TMP/bad/sample.inc" <<'EOF'
stock BadFormat(playerid, params[])
{
    new buf[128];
    format(buf, sizeof(buf), params);
    return 1;
}
stock BadSql(playerid, name[])
{
    mysql_format(gDb, gQuery, sizeof(gQuery), "SELECT * FROM players WHERE name = '%s'", name);
    return 1;
}
EOF

  cat > "$TMP/good/sample.inc" <<'EOF'
stock GoodFormat(playerid, params[])
{
    new buf[128];
    format(buf, sizeof(buf), "%s", params);
    return 1;
}
stock GoodSql(playerid, name[])
{
    mysql_format(gDb, gQuery, sizeof(gQuery), "SELECT * FROM players WHERE name = '%e'", name);
    return 1;
}
EOF

  BAD=$(grep -rnE 'format[[:space:]]*\([^,]*,[^,]*,[[:space:]]*(params|inputtext|cmdtext|dialogText)[[:space:]]*[,)]' "$TMP/bad" | wc -l)
  GOOD=$(grep -rnE 'format[[:space:]]*\([^,]*,[^,]*,[[:space:]]*(params|inputtext|cmdtext|dialogText)[[:space:]]*[,)]' "$TMP/good" | wc -l)
  BADSQL=$(grep -rnE 'mysql_format[[:space:]]*\(.*"[^"]*%s' "$TMP/bad" | wc -l)
  GOODSQL=$(grep -rnE 'mysql_format[[:space:]]*\(.*"[^"]*%s' "$TMP/good" | wc -l)
  rm -rf "$TMP"

  local_ok=1
  [[ $BAD -eq 1 ]] || { echo "[LINT] SELF-TEST GAGAL: format-injection tidak menangkap sampel buruk"; local_ok=0; }
  [[ $GOOD -eq 0 ]] || { echo "[LINT] SELF-TEST GAGAL: format-injection menandai sampel aman"; local_ok=0; }
  [[ $BADSQL -eq 1 ]] || { echo "[LINT] SELF-TEST GAGAL: mysql-percent-s tidak menangkap sampel buruk"; local_ok=0; }
  [[ $GOODSQL -eq 0 ]] || { echo "[LINT] SELF-TEST GAGAL: mysql-percent-s menandai sampel aman"; local_ok=0; }
  if [[ $local_ok -eq 1 ]]; then
    echo "[LINT] self-test 4/4 lulus (2 positif, 2 negatif)"
    exit 0
  fi
  exit 1
fi

# ---------------------------------------------------------------------------
# Scan
# ---------------------------------------------------------------------------
run_rules "$TARGET"

if [[ ${#FINDINGS[@]} -eq 0 ]]; then
  echo "[LINT] keamanan statis: 0 temuan (format-injection, mysql-percent-s, dialog-unguarded, poolsize-loop, weak-rcon)"
  exit 0
fi

for f in "${FINDINGS[@]}"; do
  IFS='|' read -r level rule detail <<< "$f"
  if [[ "$level" == "ERROR" ]]; then
    printf '\033[31m[LINT] %-18s %s\033[0m\n' "$rule" "$detail" >&2
  else
    printf '\033[33m[LINT] %-18s %s\033[0m\n' "$rule" "$detail" >&2
  fi
done

printf '[LINT] ringkasan: %d error, %d warning\n' "$ERRORS" "$WARNINGS" >&2

if [[ $ERRORS -gt 0 && $WARN_ONLY -eq 0 ]]; then
  printf '\033[31m[LINT] GAGAL: %d error keamanan harus diperbaiki sebelum build\033[0m\n' "$ERRORS" >&2
  exit 1
fi
exit 0
