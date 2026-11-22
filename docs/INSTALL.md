# Panduan Instalasi & Operasi — LAST STAND: San Andreas Quarantine

Dokumen ini menuntun dari mesin kosong sampai server hidup 24/7, plus prosedur
operasi harian (backup, soak test, restart aman, troubleshooting).

Target runtime yang **terverifikasi**: SA-MP 0.3.7 (`samp03svr` 32-bit) + plugin
yang dipatok di `pawn.json`. open.mp masih rencana migrasi, bukan jalur produksi.

---

## 1. Prasyarat

| Komponen | Versi teruji | Catatan |
|---|---|---|
| Linux x86_64 | Ubuntu 22.04/24.04, Debian 12 | `samp03svr` 32-bit butuh `libc6:i386`, `libstdc++6:i386` |
| Docker + Compose | 29.x | untuk MariaDB 10.11 |
| sampctl | terbaru | build, dependency pin, plugin fetch |
| pawncc | 3.10.10 | dibundel sampctl |
| Python 3.11+ | `requests`, `PyRakSamp` | hanya untuk load test (`tools/loadtest`) |
| RAM | ≥ 2 GB | RSS produksi 500–900 MB |
| Port | 7777/UDP | wajib terbuka; RCON/TCP tidak dipakai publik |

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y libc6:i386 libstdc++6:i386 libbz2-1.0:i386 zlib1g:i386 \
                    docker.io docker-compose-plugin python3 python3-pip
# sampctl
curl -sSL https://raw.githubusercontent.com/Southclaws/sampctl/master/install.sh | bash
```

---

## 2. Ambil kode & dependency

```bash
git clone https://github.com/FerzDevZ/zombie-samp.git
cd zombie-samp
sampctl package ensure      # tarik pawn-stdlib, YSI 5, streamer, MySQL, sscanf, bcrypt, FCNPC
bash scripts/ci-fetch-plugins.sh   # taruh .so yang dipatok ke plugins/
```

`dependencies/`, `plugins/*.so`, `samp03svr`, `scriptfiles/colandreas/*.cadb`
**tidak** ikut repo (lihat `.gitignore`): ukurannya ratusan MB dan bisa
di-generate ulang. `sampctl package ensure` + `ci-fetch-plugins.sh` mencukupi.

Data collision ColAndreas (54 MB) diunduh terpisah:

```bash
mkdir -p scriptfiles/colandreas
curl -L -o cadb.zip https://github.com/Southclaws/ColAndreas/releases/download/v1.5.0/colandreas.zip
unzip -o cadb.zip -d scriptfiles/colandreas/
ln -sfn colandreas scriptfiles/ColAndreas   # FCNPC membaca scriptfiles/colandreas/ColAndreas.cadb
```

---

## 3. Database

```bash
docker compose up -d          # MariaDB 10.11 di 127.0.0.1:3306
docker compose ps
```

Migrasi `database/migrations/*.sql` (001–021) otomatis dijalankan saat volume
pertama kali dibuat. Untuk database yang sudah ada:

```bash
for f in database/migrations/*.sql; do
  docker exec -i laststand-mysql mariadb -ulaststand -pchange_me laststand < "$f"
done
```

Kredensial **jangan** ditulis di kode. Salin contoh lalu isi:

```bash
cp mysql.ini.example mysql.ini
$EDITOR mysql.ini        # hostname, username, password, database
cp mysql.ini scriptfiles/mysql.ini   # dibaca Tx_Boot (fopen dari PAWN)
```

Ganti password default sebelum produksi:

```sql
ALTER USER 'laststand'@'%' IDENTIFIED BY '<password-baru>';
```

---

## 4. Build

```bash
sampctl build
```

Gate build yang dipakai di repo ini:

* **0 error**.
* 1 warning tersisa dan boleh: `include/colandreas.inc:603 (warning 234)`
  deprecation milik dependency, bukan kode gamemode.
* `gamemodes/main.amx` terbit.

Kalau muncul `error 017: undefined symbol`, biasanya `#include` modul baru belum
ditambahkan di `gamemodes/main.pwn` atau nama simbol > 31 karakter (batas Pawn).

---

## 5. Jalankan server

### Mode cepat (satu instans, foreground)

```bash
bash scripts/run-server.sh
```

### Mode multi-instans (disarankan)

```bash
./scripts/instance.sh init s1      # buat runtime/s1 + tautan file bersama
./scripts/instance.sh start s1
./scripts/instance.sh status s1
./scripts/instance.sh logs s1
./scripts/instance.sh restart s1
./scripts/instance.sh stop s1
```

`runtime/<instans>/` berisi `server.cfg` hasil generate, `server.pid`,
`server_log.txt`, `console.log`. File bersama (`ColAndreas.cadb`, `nodes`,
`admin_keys.cfg`, `mysql.ini`) ditautkan ulang setiap `start`, jadi aman
menambah file runtime baru di root `scriptfiles/`.

### Supervisor 24/7 (WAJIB di mesin ini)

Cron dan `systemd --user` **membunuh** `samp03svr` 32-bit dengan `SIGSYS`
(sandbox seccomp). Supervisor yang terbukti bekerja adalah skrip shell dari sesi
shell:

```bash
setsid nohup ./scripts/supervise.sh s1 >> backups/supervisor.log 2>&1 < /dev/null & disown
pgrep -af supervise.sh
```

Uji gate-nya:

```bash
kill -9 "$(cat runtime/s1/server.pid)"   # harus hidup lagi ≤ 40 detik
tail -f backups/supervisor.log
```

Supervisor juga memantau RSS; di atas `MEM_LIMIT_MB` (default 1200) server
di-restart terencana sebelum kehabisan ruang alamat 32-bit.

### Kunci admin runtime

```bash
$EDITOR scriptfiles/admin_keys.cfg     # tidak masuk repo, rotasi tanpa compile
```

Isi: satu kunci per baris. Dipakai lewat `/admin <kunci>` dan
`/superadmin <kunci>`; level tersimpan di `accounts.admin_level`.

---

## 6. Verifikasi setelah boot

Cek log `runtime/s1/server_log.txt`:

```
[SELFTEST] 36/36 PASS, 0 FAIL
[PERF] tick ms (akhir/puncak): zombie=… director=… | zombie: alive=… atk=…
```

| Yang dicek | Nilai sehat |
|---|---|
| SELFTEST | `36/36 PASS, 0 FAIL` |
| Boot time | 30–35 detik |
| RSS idle (tanpa pemain) | ~498 MB |
| `[PERF] tick` p95 semua sistem | < 20 ms |
| `errors.log` | tidak bertambah |

Smoke test in-game (PC atau Android):

1. `/daftar passwordku` → `/masuk passwordku` → pilih karakter.
2. `/interact` di hotspot hub, ambil kit, `/craft`, `/hs 1`.
3. Jalan keluar hub (radius 180), biarkan zombie mendekat, tembak.
4. Mati → `/revivekan` oleh teman atau tunggu respawn.
5. `/ekstrak` di titik ekstraksi dengan sampel di tas.

---

## 7. Operasi harian

### Backup (DB + file runtime)

```bash
bash scripts/backup.sh
ls backups/           # dump SQL + SHA256SUMS
bash scripts/restore.sh backups/<arsip>/
```

Jadwalkan tiap 6 jam lewat cron **untuk backup saja** (bukan supervisor):
`0 */6 * * * cd /srv/zombie-samp && bash scripts/backup.sh`.

### Soak test sebelum rilis

```bash
STAGGER=0.9 ./scripts/soak.sh 10 100 s1      # 10 menit, 100 bot
STAGGER=0.9 ./scripts/soak.sh 60 100 s1      # gate go-live
```

Hasil di `runtime/soak/<stempel>/summary.md`. Gate lulus = semua baris PASS:
server hidup, 0 assertion, spawn bot ≥ 90%, p95 < 20 ms, pertumbuhan RSS < 15%.
`SPREAD=1` (default) menyebar bot ke 8 zona — **wajib**, karena biaya sync NPC
ditentukan kepadatan zona, bukan jumlah pemain.

### Probe memori

```bash
tools/loadtest/mem_probe.sh 30 120          # 30 bot, 120 detik, restart dulu
tools/loadtest/mem_probe.sh 100 150 restart:0   # pakai server yang sedang jalan
```

Acuan terukur: idle 498 MB, **0,58–0,80 MB per pemain**, biaya NPC dibayar
saat NPC ter-stream (`≈ 0,7 MB per NPC parkir`).

---

## 8. Troubleshooting

| Gejala | Penyebab & solusi |
|---|---|
| `SIGSYS` / `Bad system call` berulang | dijalankan dari cron/systemd → pakai `scripts/supervise.sh` dari sesi shell |
| Server mati saat 100+ pemain | kehabisan ruang alamat 32-bit; cek RSS, turunkan NPC, restart terjadwal supervisor |
| `FCNPC] Error: NPC not created. maxplayers limit` | slot pemain+NPC penuh → pastikan `maxplayers > maxnpc + 8`, jangan `FCNPC_Create` tanpa `Npc_SlotAvailable()` |
| `Assertion 'objectExists==true' failed` (RakNet) | `FCNPC_Create` gagal setengah jalan; sama seperti di atas |
| Crash `_IO_vsprintf` | format string dari data, atau destroy NPC di dalam callback; lihat `MEMORY.md` (`Logging/Format`, `Destroy/Deferred`, `NPC/ParkKill`) |
| Pemain mobile tak bisa login | klien mobile mengirim input kosong untuk dialog input → semua auth lewat chat (`/masuk`, `/daftar`) |
| Zombie tidak menyerang saat load test | bot tidak benar-benar spawn → harness wajib `RPC_SPAWN` + `setbotposition` + `MoveToCoord` (lihat `MEMORY.md`) |
| `[ADMIN] admin_keys.cfg tidak ada` padahal ada | tautan bersama tidak dibuat → `instance.sh restart` (link_shared dijalankan di `cmd_start`) |
| Dialog tidak muncul di Android | pastikan `DIALOG_STYLE_TABLIST_HEADERS`, header di baris pertama, navigasi di baris terakhir |

Diagnosa crash FCNPC: lihat `runtime/s1/fcnpc-crash_*.txt` (backtrace + versi
plugin), lalu cari entri terkait di `MEMORY.md` sebelum menebak.

---

## 9. Struktur direktori

```
gamemodes/main.pwn              entry point, callback SA-MP, timer
gamemodes/modules/              77 modul .inc per domain
  account/    auth bcrypt, karakter, wizard 8 langkah, autosave
  inventory/  inventory, stash, hotbar, bag, container lock
  zombie/     core, AI, spawn director, horde, stats
  combat/     damage pipeline, wounds, LOS ColAndreas
  world/      zones, shelter, bases, vehicles, garage, mapping, lobbies
  player/     state, party, party stash, extraction, needs, tutorial
  progression/skills, contracts, leaderboard
  economy/    vendor, trade
  admin/      commands, panel, moderation, roles, superadmin
  ui/         inventory_ui, craft_ui
  core/       perf, spatial_hash, npc_budget, zombie_stats, selftest
database/migrations/            21 migrasi SQL
scripts/                        instance, supervise, soak, backup, restore, ci
tools/loadtest/                 bot_run.py (PyRakSamp), mem_probe.sh
docs/                           PLAN_NEXT, PLAN_GO_LIVE, INSTALL, COMMANDS, dll
runtime/<instans>/              artefak runtime (tidak masuk repo)
```

---

## 10. Checklist go-live

- [ ] `sampctl build` 0 error, hanya warning 234 bawaan ColAndreas
- [ ] `SELFTEST 36/36 PASS`
- [ ] `./scripts/soak.sh 60 100 s1` → LULUS
- [ ] Bot load test terbukti `pos ≠ 0,0,0`, `atk > 0`, `dekat < 5 m`
- [ ] `mysql.ini` & `admin_keys.cfg` berisi kredensial baru (bukan contoh)
- [ ] Firewall: **hanya 7777/UDP** terbuka; 3306 tertutup dari publik
- [ ] `scripts/supervise.sh` jalan, uji `kill -9` pulih ≤ 40 detik
- [ ] Backup otomatis + uji `scripts/restore.sh` sekali
- [ ] Playtest PC + Android (dialog, hotbar, revive, ekstraksi)
