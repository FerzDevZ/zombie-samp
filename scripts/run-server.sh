#!/usr/bin/env bash
# Jalankan server dengan LD_LIBRARY_PATH yang benar untuk log-core.so (dependensi mysql.so).
set -euo pipefail
cd "$(dirname "$0")/.."
export LD_LIBRARY_PATH="$PWD/plugins:$PWD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Tx_Boot membaca kredensial dari scriptfiles (sandbox fopen PAWN).
cp -f mysql.ini scriptfiles/mysql.ini
exec sampctl run "$@"
