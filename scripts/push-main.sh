#!/usr/bin/env bash
# Push main ke GitHub tanpa menyimpan token di disk/history.
# Token dibaca dari stdin (tidak di-echo, tidak masuk ~/.bash_history karena
# dibaca via `read -rs`, bukan sebagai argumen).
#
# Pakai:  bash scripts/push-main.sh
#         bash scripts/push-main.sh --dry-run

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

REMOTE="${REMOTE:-origin}"
BRANCH="${BRANCH:-main}"
GH_USER="${GH_USER:-FerzDevZ}"
DRY_RUN=0
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=1

if ! git remote get-url "$REMOTE" >/dev/null 2>&1; then
    echo "GAGAL: remote '$REMOTE' tidak ada" >&2
    exit 1
fi

local_head="$(git rev-parse HEAD)"
local_branch="$(git rev-parse --abbrev-ref HEAD)"
echo "repo   : $REPO_ROOT"
echo "branch : $local_branch -> $REMOTE/$BRANCH"
echo "commit : $local_head"
echo "di depan remote: $(git rev-list --count "$REMOTE/$BRANCH..HEAD" 2>/dev/null || echo '?') commit"
echo

printf 'Token GitHub (input tersembunyi, tidak akan ditampilkan): ' >&2
IFS= read -rs GH_TOKEN
printf '\n' >&2

if [[ -z "${GH_TOKEN:-}" ]]; then
    echo "GAGAL: token kosong" >&2
    exit 1
fi

cleanup() { unset GH_TOKEN; }
trap cleanup EXIT INT TERM

push_args=(push "$REMOTE" "HEAD:$BRANCH")
[[ $DRY_RUN -eq 1 ]] && push_args+=(--dry-run)

# credential.helper kosong + helper inline: token hanya hidup di proses ini,
# tidak ditulis ke ~/.git-credentials maupun config git.
if git -c credential.helper= \
       -c "credential.helper=!f() { echo username=$GH_USER; echo password=\$GH_TOKEN; }; f" \
       -c credential.interactive=never \
       "${push_args[@]}"; then
    echo
    if [[ $DRY_RUN -eq 1 ]]; then
        echo "DRY-RUN OK: push akan berhasil."
        exit 0
    fi
    remote_head="$(git ls-remote "$REMOTE" "refs/heads/$BRANCH" | awk '{print $1}')"
    echo "remote/$BRANCH : $remote_head"
    if [[ "$remote_head" == "$local_head" ]]; then
        echo "VERIFIKASI OK: remote sama dengan HEAD lokal."
    else
        echo "PERINGATAN: remote ($remote_head) != HEAD lokal ($local_head)" >&2
        exit 1
    fi
    echo
    echo "SEKARANG: revoke token itu di https://github.com/settings/tokens"
    echo "lalu rotate: rcon s1/s2, password MariaDB laststand + root, super key admin_keys.cfg"
else
    echo "GAGAL: push ditolak (token salah/kedaluwarsa/tanpa scope 'repo')" >&2
    exit 1
fi
