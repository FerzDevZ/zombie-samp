#!/usr/bin/env bash
# Ambil plugin linux untuk CI/selftest lokal dari rilis resmi (URL diverifikasi
# 26-11-19 via API GitHub). plugins/*.so di-gitignore, jadi CI wajib fetch.
# Pakai: ./scripts/ci-fetch-plugins.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p plugins .ci-tmp

fetch() { # fetch <url> <nama-file-hasil>
    local url="$1" out="$2"
    echo "[CI-PLUGINS] $url"
    curl -sL --fail --retry 3 --max-time 120 "$url" -o ".ci-tmp/$out"
}

extract_so() { # extract_so <arsip> <nama-so> — cari .so di dalam arsip, salin ke plugins/
    local archive="$1"
    local soname="$2"
    local dir=".ci-tmp/x_$soname"
    rm -rf "$dir"; mkdir -p "$dir"
    case "$archive" in
        *.zip) unzip -q -o ".ci-tmp/$archive" -d "$dir" ;;
        *.tar.gz) tar xzf ".ci-tmp/$archive" -C "$dir" ;;
        *.so) cp ".ci-tmp/$archive" "plugins/$soname"; return 0 ;;
    esac
    local found
    found="$(find "$dir" -name "$soname" -type f | head -1)"
    if [ -z "$found" ]; then
        echo "[CI-PLUGINS] GAGAL: $soname tidak ditemukan di $archive" >&2
        find "$dir" -type f | head -10 >&2
        exit 1
    fi
    cp "$found" "plugins/$soname"
}

fetch "https://github.com/pBlueG/SA-MP-MySQL/releases/download/R41-4/mysql-R41-4-Debian.tar.gz" mysql.tar.gz
extract_so mysql.tar.gz mysql.so
# log-core.so dependensi mysql R41 (LD_LIBRARY_PATH=plugins saat run).
core="$(find .ci-tmp/x_mysql.so -name 'log-core.so' -type f | head -1)"
[ -n "$core" ] && cp "$core" plugins/log-core.so

fetch "https://github.com/samp-incognito/samp-streamer-plugin/releases/download/v2.9.6/samp-streamer-plugin-2.9.6.zip" streamer.zip
extract_so streamer.zip streamer.so

fetch "https://github.com/ziggi/FCNPC/releases/download/v2.0.11/FCNPC-2.0.11.zip" fcnpc.zip
extract_so fcnpc.zip FCNPC.so

fetch "https://github.com/maddinat0r/sscanf/releases/download/v2.13.8/sscanf-2.13.8-linux.tar.gz" sscanf.tar.gz
extract_so sscanf.tar.gz sscanf.so

fetch "https://github.com/Sreyas-Sreelal/samp-bcrypt/releases/download/0.4.1/samp-bcrypt-linux-x86.zip" bcrypt.zip
extract_so bcrypt.zip samp_bcrypt.so

fetch "https://github.com/Pottus/ColAndreas/releases/download/1.5.0/ColAndreas.so" ColAndreas.so
extract_so ColAndreas.so ColAndreas.so

rm -rf .ci-tmp
echo "[CI-PLUGINS] selesai:"
ls -la plugins/*.so
