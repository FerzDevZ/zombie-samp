#!/usr/bin/env bash
# Jalankan server di bawah gdb supaya SIGSEGV/SIGABRT meninggalkan backtrace.
# SA-MP mematikan core dump lewat prctl, jadi `ulimit -c unlimited` saja tidak
# menghasilkan core (terbukti: dua kematian tanpa file core).
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="${1:-s1}"
DIR="$ROOT/runtime/$NAME"
cd "$DIR" || exit 1
exec gdb -batch -nx \
    -ex "set pagination off" \
    -ex "set print thread-events off" \
    -ex "handle SIGPIPE nostop noprint pass" \
    -ex "run" \
    -ex "echo \n=== SIGNAL ===\n" \
    -ex "bt 12" \
    -ex "frame 3" \
    -ex "echo \n=== FORMAT STRING ===\n" \
    -ex 'x/s *(char**)($ebp+0x10)' \
    -ex "echo \n=== ARG WORDS ===\n" \
    -ex 'x/10wx $ebp+8' \
    -ex "info registers eip" \
    -ex "thread apply all bt 10" \
    --args ./samp03svr
