# PLAN GO-LIVE — LAST STAND (Fase 8 → server hidup 24/7)

Dokumen ini adalah rencana eksekusi berurutan dari kondisi saat ini (server lokal jalan,
build bersih, selftest 36/36) sampai **server online stabil dan ditinggal jalan**.

Aturan main seluruh rencana:

1. Setiap fase punya **gate numerik** — tidak ada fase dianggap selesai tanpa bukti log/berkas.
2. Tidak ada klaim "sudah bagus" tanpa angka: tick ms, RSS, jumlah query, assertion, spawn rate.
3. Bug/insiden yang lolos gate dicatat di `MEMORY.md` (format symptom → root cause → invariant).
4. Stack tetap: SA-MP 0.3.7 + YSI 5 + MySQL R41-4 + Streamer + FCNPC + ColAndreas + bcrypt.
5. Target klien: PC **dan** SA-MP Mobile (Android) — setiap fitur wajib punya fallback command.

---

## Fase A — Preflight (≈30 menit)

Tujuan: memastikan apa pun yang diukur di Fase B tidak tercemar konfigurasi salah.

| # | Langkah | Perintah | Gate |
|---|---|---|---|
| A1 | Build bersih | `sampctl build` | exit 0, 0 error, ≤1 diagnostik pihak ketiga |
| A2 | Boot + selftest | `./scripts/instance.sh restart s1` lalu tunggu 25 dtk | `[SELFTEST] 36/36 PASS, 0 FAIL` |
| A3 | Kunci admin aktif | baca log boot | `[ADMIN] kunci admin dimuat dari admin_keys.cfg` |
| A4 | Paritas slot | `grep -E '^maxplayers|^maxnpc' runtime/s1/server.cfg pawn.json` | `maxplayers 300`, `maxnpc 200`, `MAX_PLAYERS 400` |
| A5 | Backup DB sebelum uji destruktif | `./scripts/backup.sh` | dump + SHA256SUMS valid, retensi ≥7 hari |
| A6 | Watchdog aktif | `crontab -l \| grep watchdog` | restart otomatis ≤1 menit setelah mati |
| A7 | Backup harian aktif | `crontab -l \| grep backup` | jadwal harian ada |
| A8 | Disk & RAM | `df -h /`, `free -m` | disk ≥20% bebas, RAM ≥8 GB tersedia |
| A9 | Log bersih | `wc -l logs/errors.log` | tidak ada baris baru saat idle 60 dtk |
| A10 | Database terhubung | log boot | `[MYSQL] tersambung ... dalam <500 ms`, `handle terpisah` (TX) |

Bukti Fase A disimpan: `runtime/soak/preflight-<stamp>.txt` (output perintah A1–A10).

---

## Fase B — Soak 60 menit, 100 pemain (≈75 menit)

Tujuan: membuktikan klaim "100 pemain stabil" dengan angka, bukan asumsi. Ini gate Fase 8 ROADMAP.

```bash
# 1) pastikan server hidup & selftest lulus (Fase A)
./scripts/instance.sh status s1

# 2) jalankan soak (bot 100 pemain + sampling 30 dtk)
nohup ./scripts/soak.sh 60 100 s1 > runtime/soak/last-soak.log 2>&1 &

# 3) pantau langsung
tail -f runtime/soak/last-soak.log
# ringkasan akhir:
cat runtime/soak/<stamp>/summary.md
```

Yang diukur setiap 30 detik (CSV: `runtime/soak/<stamp>/samples.csv`):
RSS MB, jumlah zombie aktif, hitungan `maxplayers limit`, assertion RakNet, baris errors.log
baru, dan tick ms sistem `zombie/director/horde/player/hud`.

### Gate Fase B

| Gate | Ambang | Alasan |
|---|---|---|
| Server tetap hidup | 60 menit tanpa mati | crash = blocker mutlak |
| Assertion RakNet / SIGABRT | 0 | sumber crash yang sudah terbukti (pool slot NPC) |
| Baris baru `logs/errors.log` | 0 | crash handler tidak boleh terpicu |
| Spawn bot | ≥90/100 | membuktikan auth+CHARSEL+spawn tahan beban |
| Tick p95 semua sistem | <20 ms | target ROADMAP (pemain tidak merasakan lag) |
| Pertumbuhan RSS | <15% RSS awal | deteksi kebocoran memori |
| Kegagalan NPC (`maxplayers limit`) | 0 | bukti `Npc_SpawnRoom()` bekerja |
| Zombie aktif saat 100 pemain | ≥80 | dunia tetap berbahaya meski slot dibagi |

### Kalau gate gagal

| Gejala | Tindakan |
|---|---|
| Mati tanpa jejak (`SERVER MATI`) | cek `runtime/s1/console.log` (assertion/fatal signal), ulangi dengan `gdb -batch -ex run -ex bt --args ./samp03svr` dari `runtime/s1` |
| RSS naik monoton | ambil 3 sampel heap (`/proc/<pid>/smaps`) → cari array global/objek shelter/streamer |
| Tick director/horde >20 ms | turunkan `DIRECTOR_BUDGET_PER_PLAYER` atau interval `Director_Tick`; ukur ulang |
| Spawn bot <90% | cek rate-limit login per-IP (bot dari 127.0.0.1) → naikkan slot IP untuk uji, bukan untuk produksi |

---

## Fase C — Analisis + hardening berbasis data (≈3 jam)

Dikerjakan hanya untuk angka yang **gagal** di Fase B; kalau semua lulus, fase ini menjadi
pembersihan opsional.

1. **Memori**: baseline saat ini ~500 MB idle, 1,1 GB @100 pemain. Kalau pertumbuhan >15%:
   audit array global `[MAX_PLAYERS]` (400 slot) yang bisa dipadatkan, pool objek shelter
   (`F10` di CODEBASE_STUDY §11f masih ditunda), dan jumlah objek streamer per klien.
2. **Query**: hitung laju query dari `logs/mysql.log` selama soak; target utilisasi pool <20%.
   Bila >50%, naikkan `pool_size` (4→8) **dan** uji ulang, jangan menebak.
3. **NPC**: kalau `maxplayers limit` muncul lagi, turunkan `NPC_SLOT_RESERVE` (8→4) atau
   naikkan `maxplayers` (300→400, biaya ±11 MB/pemain).
4. **Log**: pastikan log tidak tumbuh tanpa batas (logrotate harian, simpan 14 hari).
5. Perubahan apa pun di fase ini harus melalui gate ulang: build + selftest + (bila menyangkut
   performa) soak pendek 10 menit dengan beban yang sama.

---

## Fase D — Mobile pass + playtest client nyata (≈1 hari, butuh perangkat)

Tidak bisa dikerjakan dari mesin build (tidak ada client SA-MP). Checklist siap pakai ada di
`tests/REGRESSION_26-11-19.md`. Fokus urutan uji:

1. Auth mobile: `/daftar`, `/masuk` via chat (tanpa dialog inputtext).
2. Wizard karakter 8 langkah (semua TABLIST_HEADERS, hanya nama yang diketik).
3. Ekonomi: loot → `/inv` → `/rapikan` → `/hs 1-5` → craft → vendor → kontrak → ekstraksi.
4. Container bersama: `/peti` dua pemain (pembuka kedua read-only), makam, stash partial.
5. Kendaraan: lockpick, BBM liter, jerrycan, garasi deploy/simpan.
6. Base: build, generator malam, raid, turret.
7. Kematian: downed, bleed-out, revive channel 6 dtk, adrenaline self-revive.
8. Command fallback: pastikan setiap interaksi punya versi chat (tanpa tombol Y).

Gate: tidak ada aksi yang hanya bisa dilakukan lewat keyboard/tombol F.

---

## Fase E — Go-live 24/7 (≈3 jam)

### E1. Proses & otomatisasi

| Komponen | Konfigurasi | Verifikasi |
|---|---|---|
| Instans | `./scripts/instance.sh start s1` (port 7777) | `status` = pid hidup |
| Supervisor | `setsid nohup ./scripts/supervise.sh s1 >> backups/supervisor.log 2>&1 &` | `kill -9` proses → hidup lagi ≤40 dtk, selftest 36/36 |
| Backup | cron harian `scripts/backup.sh` | dump baru + `SHA256SUMS` cocok |
| Log rotation | logrotate `runtime/*/server_log.txt`, `logs/*.log`, 14 hari | `logrotate -d` bersih |
| Restart aman | `instance.sh restart s1` (save karakter saat shutdown) | karakter & inventory utuh setelah restart |

### E1b. Kenapa BUKAN cron/systemd (terverifikasi di mesin ini)

`samp03svr` adalah biner 32-bit. Bila dinyalakan dari cron atau `systemd --user`, kernel
mematikannya dengan **SIGSYS** (`status=31/SYS`) karena sandbox seccomp pemanggil:

- `backups/watchdog.log`: `Bad system call` pada setiap restart dari cron → cron watchdog
  dinonaktifkan (komentar di crontab).
- `journalctl --user -u laststand-s1`: `Main process exited, code=killed, status=31/SYS`
  berulang saat unit systemd dicoba (unit sudah dihapus, tidak dipakai).

Yang bekerja: supervisor yang dijalankan **dari sesi shell** (mewarisi profil seccomp yang
mengizinkan biner 32-bit), lalu ia memanggil `scripts/instance.sh start` saat proses mati.
Uji nyata: `kill -9` pada server → restart otomatis dalam 40 dtk (log
`backups/supervisor.log` restart #2), boot berikutnya `[SELFTEST] 36/36 PASS`.

Konsekuensi operasional: supervisor harus dijalankan ulang setelah sesi/reboot hilang;
perintahnya ada di README bagian operasi. Untuk produksi jangka panjang, jalankan server di
VPS tanpa sandbox seccomp (atau systemd system-level, bukan `--user`).

### E2. Jaringan

| Jalur | Langkah | Gate |
|---|---|---|
| Tailscale (utama, privat) | pastikan `tailscale status` online, server `100.122.225.77` | client Tailscale bisa connect 7777 |
| playit.gg (publik, opsional) | tunnel UDP aktif, IP:port dicatat | query server via `samp-query` dari luar |
| Firewall | hanya 7777/UDP + SSH yang terbuka | `ss -lunp` tidak ada port tak perlu |

### E3. Keamanan operasional

1. Rotasi `scriptfiles/admin_keys.cfg` sebelum publik (kunci baru, chmod 600, symlink instans).
2. Pastikan `DEV_MODE 0`, `AUTH_INPUT_DIALOGS 0`, dan login kunci dibatasi 5 percobaan + blokir 60 dtk.
3. Verifikasi akun owner bootstrap benar (superadmin dari DB, bukan nickname hardcode).
4. Simpan salinan `admin_keys.cfg` di backup terenkripsi (restore.sh sudah menanganinya).

### E4. Monitoring ringan

- Tiap 60 menit: cek `[PERF]` baris terakhir + `wc -l logs/errors.log` (harus tetap).
- Tiap hari: baca `backups/watchdog.log` (hitungan restart harus 0; restart berulang = sinyal crash).
- `/diag`, `/selftest`, `/stats` dijalankan admin setelah deploy besar.

### E5. Rollback

1. `./scripts/restore.sh <dump>` (DB) — verifikasi jumlah tabel + karakter sample.
2. `git`/salinan `gamemodes/` sebelumnya (tidak ada repo git: simpan tar `gamemodes-<stamp>.tar.gz`
   sebelum tiap deploy).
3. `./scripts/instance.sh restart s1` lalu ulangi Fase A.

### Gate E (Definition of Done — "server on")

- [ ] Server hidup ≥60 menit pasca-deploy tanpa restart watchdog.
- [ ] `[SELFTEST] 36/36 PASS` pada boot terakhir.
- [ ] `logs/errors.log` tidak bertambah selama ≥60 menit.
- [ ] Bisa connect dari perangkat luar (Tailscale; playit bila dipakai).
- [ ] Backup harian ada dan sudah diuji restore sekali.
- [ ] Watchdog terbukti menghidupkan ulang proses saat dimatikan manual.
- [ ] Rencana rollback tertulis + tar `gamemodes` tersimpan.

---

## Ringkasan jadwal

| Fase | Durasi | Gate utama | Status |
|---|---|---|---|
| A. Preflight | 30 mnt | build 0 error, selftest 36/36, backup ada | siap dijalankan |
| B. Soak 100 pemain | 75 mnt | 0 crash, p95 <20 ms, ≥90 spawn, RSS stabil | siap dijalankan |
| C. Hardening data-driven | 3 jam | gate ulang setelah tiap perubahan | kondisional |
| D. Mobile + playtest | 1 hari | semua aksi punya fallback chat | butuh perangkat |
| E. Go-live 24/7 | 3 jam | checklist E lulus semua | setelah B & D |

## Risiko yang masih terbuka

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Crash FCNPC/RakNet model lain muncul lagi | server mati | watchdog + soak berkala + simpan `console.log` tiap insiden |
| 190 zombie + 100 pemain tidak muat di 300 slot | kepadatan zombie turun | budget otomatis `Npc_SpawnRoom()`; naikkan `maxplayers` bila perlu |
| UI/dialog belum diverifikasi visual | bug UX tak terlihat | Fase D wajib sebelum publik |
| Query menumpuk saat 100 pemain login serentak | login lambat | `pool_size`, autosave stagger, ukur di Fase B |
| Item ledger tumbuh selamanya | DB membesar | agregasi harian/arsip >180 hari (belum dikerjakan) |
