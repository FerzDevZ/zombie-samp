#!/usr/bin/env bash
# Hentikan supervisor + server tanpa pgrep -f (pola -f bisa cocok dengan
# command line pemanggil sendiri sehingga shell ikut terbunuh).
set -uo pipefail
for pidDir in /proc/[0-9]*; do
    pid="${pidDir#/proc/}"
    [ "$pid" = "$$" ] && continue
    cmd="$(tr '\0' ' ' < "$pidDir/cmdline" 2>/dev/null)"
    case "$cmd" in
        *samp03svr*|*supervise.sh*|*bot_run.py*|*gdb\ -batch*)
            kill "$pid" 2>/dev/null
            ;;
    esac
done
sleep 2
ss -lun 2>/dev/null | grep -q ":7777 " && echo "port 7777 masih dipakai" || echo "port 7777 bebas"
