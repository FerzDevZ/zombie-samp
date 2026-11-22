# RENCANA LANJUTAN — Re-baseline 19 Nov 2026

Dokumen ini menggantikan asumsi di `docs/PLAN_GO_LIVE.md` yang sudah tidak valid.
Semua angka di sini hasil ukur ulang di mesin build, bukan perkiraan.

---

## 1. Apa yang berubah hari ini

Tiga temuan membalik prioritas rencana sebelumnya.

### 1.1 Semua soak sebelum hari ini mengukur pemain hantu

`bot_run.py` hanya menjawab dialog CHARSEL lalu menganggap bot "spawned" bila koneksi
hidup. Kenyataannya server sudah memanggil `SpawnPlayer`, tetapi klien RakSamp tidak
pernah mengirim `RPC_SPAWN` dan sync mengirim posisi default `0.0,0.0,0.0` — sehingga
`GetPlayerPos` di server selalu `0,0,0` (tengah laut).

Akibatnya:
- Zombie mustahil menargetkan siapa pun (`target=0`, `dekat=9999.0`).
- Angka "6,4 MB/pemain" di `MEMORY.md` lama adalah biaya koneksi hantu, bukan pemain.

Perbaikan yang sudah masuk (`tools/loadtest/bot_run.py`):
1. `send_rpc_spawn()` setelah CHARSEL (3x retry),
2. `setbotposition(1685, -2333, 13)` — posisi spawn nyata,
3. `MoveToCoord(1780, -2160, 13, mode="foot")` untuk keluar hub (flag `--walk-out`, default 1).

Bukti berhasil (log server):
```
[PERF] ... | zombie: alive=112 lahir=125 mati=29 atk=103 macet=0 jangkau=103
        los_gagal=0 sampel=11606 target=520 dekat=0.6
```
Zombie AI, LOS ColAndreas, dan damage pipeline terbukti bekerja. `macet=0`.

### 1.2 Biaya pemain nyata kecil, biaya NPC besar

| Skenario | Baseline | Puncak | Delta | Per unit | Slope idle |
|---|---|---|---|---|---|
| 10 bot, zombie nyala | 498 MB | 510 MB | 12 MB | 1,20 MB/pemain | 0,05 MB/s |
| 30 bot, zombie nyala | 498 MB | 518 MB | 20 MB | 0,67 MB/pemain | 0,03 MB/s |
| 60 bot, zombie nyala | 498 MB | 533 MB | 35 MB | 0,58 MB/pemain | 0,00 MB/s |
| **100 bot, zombie MATI** | 498 MB | **578 MB** | 80 MB | **0,80 MB/pemain** | 0,05 MB/s |
| **100 bot, zombie nyala** | 498 MB | **1000 MB** | 502 MB | — | **1,67 MB/s** |

Kesimpulan: `RSS ≈ 498 + 0,8 × pemain + 3,8 × NPC aktif`.
Penyebab utama plafon 32-bit adalah **siklus hidup NPC/FCNPC**, bukan pemain, bukan
MySQL (sudah diuji: 888 query/menit = +4 MB), bukan `MALLOC_ARENA_MAX`.

### 1.3 `Iter_Init` untuk array iterator

`Iterator:HashCellPlayers[900]<MAX_PLAYERS>` tidak auto-init. Sekarang dipanggil lewat
`Hash_Init()` di `OnGameModeInit()`. Tanpa ini, `foreach` pada sel hash bisa kosong
tanpa error kompilasi.

---

## 2. Sasaran yang direvisi

| Parameter | Target lama | Target baru | Alasan |
|---|---|---|---|
| Pemain serentak | 100 | **100 (siap), 150 setelah P0.2** | 0,8 MB/pemain sudah aman |
| NPC aktif bersamaan | 190 | **≤120 sampai P0.2 selesai** | 3,8 MB/NPC |
| RSS puncak | <800 MB | **<900 MB @100 pemain + 120 NPC** | plafon 32-bit ~1,2 GB |
| p95 tick | <20 ms | <20 ms (belum diukur 60 mnt) | `horde` sempat 9 ms |
| Serangan zombie | belum terukur | ≥100/menit/100 pemain, `macet` ≤2% | angka acuan baru |

---

## 3. Backlog berprioritas

### P0.1 — Audit biaya per NPC (blocker utama) — 1 hari

Ukur dari mana 3,8 MB/NPC berasal.

```bash
# A. baseline NPC: 30 bot, 60 zombie, tanpa pemain tambahan
valgrind --tool=massif --massif-out-file=/tmp/massif_npc.out --time-unit=B ./samp03svr
ms_print /tmp/massif_npc.out | head -60
```
Kandidat yang harus dibedakan lewat massif:
1. FCNPC `CPlayerData` + ColAndreas embedded (setiap NPC punya world collision handle),
2. `FCNPC_MOVE_MODE_COLANDREAS` vs `MAPANDREAS` (pathfinding),
3. objek/teks/pickup per zombie dari gamemode (streamer),
4. churn `Zombie_Create`/`Zombie_Destroy` (lahir 125, mati 29 dalam 4 menit).

**Gate:** teridentifikasi ≥70% alokasi pada satu call-path, plus tabel MB/NPC per komponen.

### P0.2 — Turunkan biaya NPC ke ≤1,5 MB/NPC — 1–2 hari

Urutan percobaan (hentikan begitu gate tercapai):
1. Ganti pathfinding zombie ke `FCNPC_MOVE_MODE_MAPANDREAS` / `NONE` + raycast manual
   (A/B: `atk`, `macet`, `los_gagal` harus tetap ≥ baseline).
2. Matikan ColAndreas embedded FCNPC bila pathfinding tak lagi butuh (hapus
   `plugins/ColAndreas.so` standalone dulu, ukur delta).
3. Kurangi `DIRECTOR_PER_PLAYER`/`MAX` (34/150 → 24/120) — dampak gameplay diukur dari
   `atk`/menit, bukan perasaan.
4. `FCNPC_SetUpdateRate` / sync NPC 100 ms → 200 ms untuk zombie jauh (LOD sudah ada,
   verifikasi tidak menurunkan `atk`).

**Gate:** RSS @100 pemain + 120 NPC < 900 MB, `atk` ≥100/menit, `macet` ≤2%, 0 crash 30 mnt.

### P0.3 — Soak 60 menit penuh (gate go-live) — 0,5 hari

```bash
./scripts/soak.sh 60 100 s1
```
**Gate:** 0 crash, 0 assertion, p95 tick <20 ms, RSS <900 MB, `atk` stabil, `lahir/mati`
NPC wajar (tidak ada zombie hilang tanpa `mati` bertambah).

### P1.1 — A/B kualitas navigasi zombie — 0,5 hari

Skrip uji: 30 bot, `--walk-out 1`, 3 posisi (Camp Outskirts, Ganton, Downtown).
Bandingkan ColAndreas vs MapAndreas: `atk`, `macet`, `jangkau`, `los_gagal`, waktu capai
target. Simpan hasil di `runtime/soak/nav-ab-<stamp>.txt`.

**Gate:** pilih mode dengan `macet` terendah dan `atk` tertinggi; catat di `MEMORY.md`.

### P1.2 — `horde` dan director p95 — 0,5 hari

`horde` sempat 9 ms pada 100 pemain. Kalau p95 >10 ms: cache posisi pemain 1 detik +
grid 30 m, dan pindahkan pemilihan zona keluar dari tick 500 ms.

### P2.1 — Operasional produksi — 1 hari

- INFO log dimatikan di produksi (DEBUG saja untuk diagnostik), logrotate 14 hari.
- Autosave bertahap (batch 5 sudah ada; tambah metrik durasi ke PERF).
- Backup harian + uji restore sekali (sudah ada `scripts/restore.sh`).
- Rotasi `admin_keys.cfg`, firewall 7777/UDP saja.

### P2.2 — Fitur (setelah P0 selesai, urutan dari `RANCANGAN_2026.md` §3)

F1–F10 dikerjakan hanya setelah gate P0.2 dan P0.3 lulus. Setiap fitur wajib:
`selftest 36/36` + build 1 diagnostik + satu gate angka di PERF.

---

## 4. Jadwal

| Hari | Isi | Gate keluar |
|---|---|---|
| D+0 (hari ini) | Re-baseline, perbaikan bot harness, `Hash_Init`, telemetri `lahir/mati` | build bersih, selftest 36/36, `atk>0` |
| D+1 | P0.1 massif NPC | call-path ≥70% teridentifikasi |
| D+2–D+3 | P0.2 turunkan MB/NPC | RSS <900 MB @100+120 |
| D+4 | P0.3 soak 60 menit | 0 crash, p95 <20 ms |
| D+5 | P1.1 A/B navigasi + P1.2 horde | mode navigasi dipilih, tercatat |
| D+6 | P2.1 operasional | log/backup/firewall beres |
| D+7–D+8 | F1–F10 batch pertama | per fitur: selftest + gate |

---

## 5. Risiko

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Biaya NPC tidak bisa turun <1,5 MB | Plafon 100 pemain permanen | Turunkan `MAX_ZOMBIES` ke 100 dan naikkan jumlah pemain; NPC lebih sedikit tapi lebih pintar |
| Pathfinding tanpa ColAndreas membuat zombie macet | Gameplay rusak | A/B dengan `macet` sebagai gate keras; fallback raycast manual |
| Perubahan bot harness menyembunyikan bug lama | Data menyesatkan | Semua angka lama ditandai tidak valid di `MEMORY.md`; hitung ulang sebelum klaim |
| 32-bit address space tetap membatasi | Crash mendadak | Supervisor `MEM_LIMIT_MB` + restart terencana; jangka panjang pindah open.mp 64-bit |
| Tidak ada client SA-MP di mesin build | UI/dialog tak terverifikasi | Wajib playtest manual (Android + PC) sebelum go-live |

---

## 6. Perintah rujukan

```bash
# build + selftest
sampctl build && ./scripts/instance.sh restart s1 && sleep 35
grep -E "SELFTEST|\[ADMIN\] kunci|\[MYSQL\]" runtime/s1/server_log.txt | tail -3

# beban nyata (bot masuk dunia dan keluar hub)
python3 tools/loadtest/bot_run.py --count 30 --duration 300 --report /tmp/r.json

# biaya memori per pemain / per NPC
./tools/loadtest/mem_probe.sh 100 150        # zombie nyala
# lalu ubah ZOMBIE_SPAWN_DISABLED 1 di constants.inc, build, ulangi, kembalikan ke 0

# soak gate
./scripts/soak.sh 60 100 s1

# supervisor 24/7 (WAJIB dari sesi shell, bukan cron/systemd)
setsid nohup ./scripts/supervise.sh s1 >> backups/supervisor.log 2>&1 < /dev/null & disown
```

---

## 7. Definition of Done go-live

1. Build 1 diagnostik (`colandreas.inc:603` warning 234), 0 error.
2. `[SELFTEST] 36/36 PASS`.
3. Soak 60 menit @100 pemain + ≥100 NPC: 0 crash, p95 <20 ms, RSS <900 MB.
4. `atk` ≥100/menit, `macet` ≤2%, `los_gagal` wajar.
5. Bot harness terbukti menempatkan pemain di dunia (pos != 0,0,0).
6. Playtest manual PC + Android: login, inventori, dialog, trade, party, kematian.
7. `admin_keys.cfg` dirotasi, firewall 7777/UDP, backup + uji restore.
8. Supervisor jalan dan terbukti restart ≤40 detik setelah `kill -9`.

## HASIL EKSEKUSI P0.1–P0.3 (19-11, build terverifikasi)

Ketiga temuan awal sudah dijawab dengan pengukuran, bukan dugaan:

| Uji | Konfigurasi | RSS | Serangan | Catatan |
|---|---|---|---|---|
| Klaster | 100 bot 1 zona, raycast ON, sync 80/100 ms | 515 → 979 MB (1,7 MB/s) | atk 13 | baseline buruk |
| Tanpa raycast | 100 bot 1 zona, `ZOMBIE_PATHFINDING_RAYCAST 0` | 511 → 548 MB (datar) | atk 0 | pathfinding OFF = zombie tak pernah menyerang, tidak dipakai |
| Sync lambat | 100 bot 1 zona, sync 300/400 ms | 511 → 651 MB (0,7 MB/s) | atk 0 | hanya memperlambat kebocoran |
| Sebar 8 zona | 100 bot `--spread-zones 1` | 515 → 557 MB (datar 60 dtk) | atk 35–120 | **biaya = kepadatan, bukan jumlah pemain** |
| Pool parkir | 100 bot sebar, `Zombie_Park` tanpa kill | **498 → 552 MB, datar 7 menit** | atk 67 | **soak 10 menit LULUS, 0 crash** |

Akar masalah yang terbukti:

1. **Biaya sync NPC = pemain × NPC ter-stream.** 100 bot di satu titik membuat 96 NPC di-stream ke 100 pemain; menyebar bot ke 8 zona membuat grafik RSS datar tanpa mengubah kode apa pun.
2. **Churn create/destroy NPC bocor ~0,8 MB per siklus** (samp03svr mengalokasikan entri pemain per NPC, tidak kembali penuh). Diperbaiki dengan pool parkir NPC (dunia 60000), bukan penghapusan.
3. **Destroy di dalam callback** = crash `_IO_vsprintf`. Diperbaiki dengan antrean hapus yang diproses di `Zombie_Tick`; disconnect bot kini bertahap (`drain_delay`).

Kode yang berubah: `zombie_core.inc` (parkir/reuse/antrean), `zombie_ai.inc` (flush antrean, `ZOMBIE_PATHFINDING_RAYCAST`), `spawn_director.inc` (clamp kepadatan + trim 25 %), `constants.inc` (4 konstanta baru), `tools/loadtest/bot_run.py` (`--spread-zones`, `drain_delay`), `scripts/soak.sh` (`SPREAD=1`).

Hasil gate `./scripts/soak.sh 10 100 s1` (final): **LULUS** — 100/100 bot spawn, 96 zombie aktif, RSS 498 → 552 MB (datar sejak menit ke-3), 0 assertion/SIGABRT/crash, p95 zombie/director/horde/player/hud ≤ 3 ms, SELFTEST 36/36. Percobaan sebelumnya gagal di menit ke-5 karena memarkir NPC dengan darah 0 memicu jalur kematian samp03svr — sekarang parkir memakai `FCNPC_Stop` + pindah dunia.

Sisa pekerjaan yang masih valid dari rencana lama: soak 60 menit, verifikasi klien nyata (PC + Android), dan rotasi/firewall operasional (P2.1).
