#!/usr/bin/env bash
# Hentikan supervisor + server TANPA pkill -f: pola -f bisa cocok dengan command
# line pemanggil sendiri sehingga shell/agent yang menjalankan ikut terbunuh
# (gejala: exit 143 tanpa keluaran). Selain $$ kita juga melewati seluruh
# rantai induk, karena perintah pemanggil biasanya memuat nama skrip ini.
set -uo pipefail

ancestors=" $$ "
walk="$PPID"
while [ -n "${walk:-}" ] && [ "$walk" != "0" ] && [ "$walk" != "1" ]; do
    ancestors="$ancestors$walk "
    walk="$(awk '/^PPid:/ {print $2}' "/proc/$walk/status" 2>/dev/null)"
done

for pidDir in /proc/[0-9]*; do
    pid="${pidDir#/proc/}"
    case "$ancestors" in
        *" $pid "*) continue ;;
    esac
    cmd="$(tr '\0' ' ' < "$pidDir/cmdline" 2>/dev/null)"
    case "$cmd" in
        *samp03svr*|*supervise.sh*|*bot_run.py*|*gdb\ -batch*|*ui_probe.py*|*fuel_probe.py*)
            kill "$pid" 2>/dev/null
            ;;
    esac
done

sleep 2
ss -lun 2>/dev/null | grep -q ":7777 " && echo "port 7777 masih dipakai" || echo "port 7777 bebas"
