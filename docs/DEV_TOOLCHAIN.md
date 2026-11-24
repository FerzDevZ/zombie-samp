# Dev Toolchain — SA-MP / open.mp 2026

Status terverifikasi: **23-11-2026**. Semua perintah di bawah dijalankan di host ini
(Ubuntu 24.04, user `aegisx`) dan hasilnya dicatat.

## Ringkas

| Komponen | Versi | Lokasi | Bukti |
|---|---|---|---|
| sampctl | 1.16.0 | `/usr/local/bin/sampctl` | `sampctl version` |
| pawncc (resmi) | 3.10.10 | `~/samp-sdk/compiler/pawnc-3.10.10-linux/bin` | `pawncc` mencetak banner |
| pawncc global | 3.10.10 | `/usr/local/bin/pawncc` (+ `pawndisasm`) | `pawncc` dari shell mana pun |
| wrapper pawncc | — | `~/samp-sdk/bin/pawncc` | cadangan per-user |
| SA-MP server | 0.3.7 | `<repo>/samp03svr` | boot `runtime/s1`, SELFTEST 36/36 PASS |
| open.mp server | 1.6.7.3218 | `~/samp-sdk/openmp/Server` | semua komponen load + script Pawn jalan |
| omp-stdlib | HEAD | `~/samp-sdk/omp-stdlib` | `#include <open.mp>` kompilasi bersih |
| crashdetect | 4.22 | `~/samp-sdk/plugins/crashdetect.so` | plugin prebuilt |
| MariaDB | 10.11 | container `laststand-mysql` | 22 tabel, 21 migrasi terpasang |
| librari i386 | libc6/libstdc++/zlib/libatomic | apt | `ldd samp03svr` bersih |

## Prasyarat 32-bit (mutlak)

`pawncc` dan `samp03svr`/`omp-server` semuanya ELF 32-bit. Tanpa loader i386,
kompilasi gagal dengan `Compiler could not start` dan server tidak bisa dijalankan.

```bash
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y libc6:i386 libstdc++6:i386 zlib1g:i386 \
  libatomic1:i386 libcurl4:i386 libssl3:i386 lib32gcc-s1 lib32stdc++6
```

## PATH

Sudah ditambahkan ke `~/.bashrc`:

```bash
export PATH="$HOME/samp-sdk/bin:$HOME/.local/bin:$PATH"
export SAMPSDK="$HOME/samp-sdk"
export LD_LIBRARY_PATH="$HOME/samp-sdk/compiler/pawnc-3.10.10-linux/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

## Build gate

```bash
./scripts/build.sh          # exit 0 = AMX layak
./scripts/build.sh --clean  # buang AMX lama dulu
```

Hasil terakhir: `main.amx` 9.377.771 byte, **1 warning, 0 error**
(warning 234 `Iter_SafeRemove_1` deprecated di `include/colandreas.inc:603`).
Skrip otomatis jatuh ke container `i386/debian` bila loader i386 hilang.

Catatan determinisme: **pawncc tidak menghasilkan byte identik antar dua kompilasi**
(dua build bersih di mesin ini: 10.415.003 byte, md5 `8b66fdf7…` vs `192c8ff8…`, beda
1 byte padding). Jadi yang boleh dijadikan bukti adalah ukuran + `0 warning 0 error`
+ gate + selftest runtime, bukan md5 hasil rebuild. md5 yang sama hanya sah untuk
membandingkan **salinan** AMX (`gamemodes/` → `runtime/s1` → `runtime/s2`).

## Audit skill & agent

```bash
python3 tools/samp_skills_audit.py   # exit 0 = semua skill terklasifikasi & gate lulus
```

"Load skill" di repo ini berarti menjalankan aturannya terhadap kode nyata. Audit
mengklasifikasi **setiap** `samp-*` skill ke salah satu dari tiga kelas: probe nyata
(grep/native/build), REFERENSI (playbook yang ditegakkan gate bernama), atau N/A dengan
bukti absen (penanda fitur 0 kemunculan). Skill yang tidak masuk kelas mana pun =
`TANPA PROBE` = exit 1. Keluaran: `docs/SKILLS_ACTIVE.md`.

## Database

```bash
docker compose up -d          # MariaDB 10.11 + migrasi otomatis
docker compose ps
```

Kredensial dev dibaca dari `.env` (dibuat dari `scriptfiles/mysql.ini`, mode 600).
`.env.example` hanya contoh.

## Menjalankan server SA-MP

```bash
./scripts/instance.sh start s1
./scripts/instance.sh logs  s1
./scripts/instance.sh stop  s1
```

`runtime/s1` dan `runtime/s2` berisi symlink absolut ke root repo. **Setiap kali
repo dipindah**, jalankan ulang `./scripts/instance.sh init|prep` atau
`scripts/build.sh` gagal karena `samp03svr` tidak ditemukan.

Bukti boot sehat (`runtime/s1/server_log.txt`):

```
[SELFTEST] 36/36 PASS, 0 FAIL
[SELFTEST] PASS tabel database | accounts=0 characters=0 ... (22 tabel)
[SELFTEST] PASS anggaran NPC rekrutan | maxnpc=200 butuh>=191, maxplayers=300
```

## open.mp (untuk gamemode baru)

```bash
cd ~/samp-sdk/openmp/Server
./omp-server
```

Kompilasi dengan stdlib open.mp:

```bash
pawncc main.pwn -omain.amx -i$HOME/samp-sdk/omp-stdlib '-;+' '-(+'
```

**Batasan penting:** gamemode LAST STAND **belum bisa** berjalan di open.mp. Hasil uji
23-11-2026 (`omp-server` 1.6.7.3218, 45 detik):

| Plugin | Status di open.mp | Bukti |
|---|---|---|
| `mysql`, `sscanf`, `streamer`, `samp_bcrypt` | OK | log "successfully loaded" |
| `ColAndreas` | **OK — tapi harus di `components/`** | `Successfully loaded component open.mp ColAndreas (1.5.0.0)` |
| `FCNPC` | **ditolak** | `Skipping legacy plugin 'FCNPC'; It requires memory hacking` |

`plugins/ColAndreas.so` yang ada di repo ini sebenarnya build **komponen open.mp**.
Kalau diletakkan di `plugins/`, open.mp hanya bilang "This file is an open.mp component.
Please move it to components/ folder" dan `CA_RayCastLine` tetap tidak terdaftar.
Setelah dipindah ke `components/`, 30 native yang tersisa tidak terdaftar **semuanya
FCNPC** (sebelumnya 34 = FCNPC + ColAndreas).

Jadi penghalang tunggal adalah FCNPC. Itu bukan tukar binary: AI zombie memakai
`FCNPC_UseMovePathfinding` + `FCNPC_MOVE_PATHFINDING_RAYCAST` +
`FCNPC_MOVE_MODE_COLANDREAS` (lihat `zombie/zombie_ai.inc`), sedangkan NPC bawaan
open.mp hanya punya `NPC_Move` / `NPC_MoveToPlayer` / `NPC_CreatePath` (waypoint
manual) tanpa pathfinding otomatis. Migrasi = menulis ulang lapisan navigasi NPC.

### Status gate dual-target (terukur 24-11-2026)

Skill `samp-pawn-compile-gate` menuntut `.pwn` lolos di **dua** set include. Repo ini
belum: target 0.3.7 (`dependencies/samp-stdlib`) = **0 warning 0 error**, target open.mp
(`/home/aegisx/samp-sdk/omp-stdlib`) = **6 error + 156 warning**. Enam error itu satu
kelas: callback yang di open.mp bertag (`WEAPON:reason`, `PLAYER_STATE:newstate`,
`BULLET_HIT_TYPE:hittype`, `BODY_PART:bodypart`) ditulis tanpa tag di `main.pwn`
(baris 488, 608, 646, 661, 682). 156 warning sisanya tag `t_WEAPON` pada
`SetSpawnInfo`/`GivePlayerWeapon` dan `GetServerVarAsInt` deprecated.

Angka itu **dikunci** oleh probe `logic-compile-omp-baseline`: turun = boleh, naik =
audit gagal. Jangan menyebut "dual-target" sampai errornya 0.

## HUD (TextDraw) — gate lebar 640 px

TextDraw tidak punya perataan kanan: baris yang kepanjangan hanya ketahuan dari
mata pemain. Karena itu setiap baris diuji dengan string **terburuk**, bukan
contoh pendek.

```bash
python3 tools/hud_preview/generate.py     # exit 1 bila ada baris meluber/drift
```

Skrip ini melakukan tiga pemeriksaan:

1. **Drift** — `x` dan `LetterSize X` di `HUD_Create` harus sama dengan tabel
   `gHudFitRows` di file yang sama, jadi angka gate tidak bisa basi.
2. **Panjang** — string terburuk di skrip harus sepanjang `hudFitWorstLen`.
3. **Muat** — `x + panjang x LetterSizeX x 35.3` harus ≤ 640.

`scripts/build.sh` menjalankan gate ini otomatis; build gagal bila melanggar.
Keluaran `tools/hud_preview/generate.html` bisa dibuka di browser sebagai
pratinjau 640x480. Metrik glif diperkirakan (35.3 px per LetterSize X, diturunkan
dari baris BBM), jadi yang divalidasi adalah geometri — posisi, tumpang-tindih,
keluar layar — bukan bentuk huruf persis di client.

## Load test

`tools/loadtest` hanya memakai stdlib Python + PyRakSamp (vendored), tanpa pip.

```bash
python3 tools/loadtest/seed_accounts.py --count 100 --spread
python3 tools/loadtest/bot_run.py --count 100 --duration 600 --spread-zones 1
./scripts/soak.sh 600 100 s1      # port dibaca dari runtime/s1/server.cfg
```

## Debugging crash

```bash
./scripts/run_gdb.sh s1           # SIGSEGV → backtrace otomatis
crashdetect.so tersedia di ~/samp-sdk/plugins untuk backtrace level AMX
```
