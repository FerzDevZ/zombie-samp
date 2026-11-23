# ROADMAP — 9 Fase Menuju Server Playable

Aturan main:
- Satu fase selesai hanya jika **gate** di kolom terakhir lulus. Tidak ada "nanti dibenerin".
- Gate compile: `sampctl package build` exit 0. Gate runtime: checklist manual in-game, ditulis di `tests/fase-N.md`.
- Setiap bug yang lolos gate ditulis ke `MEMORY.md` sebagai invariant.

| Fase | Fokus | Output | Gate |
|---|---|---|---|
| 0 | Tooling & skeleton | `pawn.json`, `config.json`, CI, 1 modul hello | `sampctl package build` exit 0 di lokal + GitHub Actions hijau |
| 1 | Framework inti | Auth bcrypt, 2 slot karakter, interaction `[Y]`, zone registry | Login → spawn → masuk zona tier 1 tercatat di DB |
| 2 | Zombie engine | FCNPC pool, LOD, spatial hash, spawn director dasar | 150 zombie berjalan tanpa tick > 20 ms (diukur `/admin diag`) |
| 3 | Combat & loot | Item registry, inventory 20 slot, loot node, damage pipeline | Tembak zombie → mati → loot masuk inventory → relog tetap ada |
| 4 | Survivability | Needs, infection, downed, revive party | Bisa downed & di-revive 2 pemain; infeksi sampai 100% jadi zombie |
| 5 | Hub & ekonomi | Stash, crafting bench, klinik, kontrak harian, cash/scrap | Ekstrak loot → stash → craft suppressor → pakai |
| 6 | Endgame loop | Barricade + safehouse, Horde Night, skill tree, XP | Horde Night menyerang shelter dan barricade hancur sesuai HP |
| 7 | Event & sosial | Airdrop, blackout, boss, party XP, market hub | 4 pemain party mengekstrak bersama dengan payout benar |
| 8 | Hardening & scale | Anti-cheat penuh, load test 100 pemain, mobile pass | 100 pemain + zombie sesuai anggaran slot, tick p95 < 20 ms, 0 crash 60 menit |

---

## Status Verifikasi (update 25-11-18)

| Item | Bukti |
|---|---|
| Compile bersih | `sampctl build` exit 0, hanya 1 warning pihak ketiga (`Iter_SafeRemove_1`) |
| Server hidup | SA-MP 0.3.7 + streamer 2.9.6 + sscanf 2.13.8 + FCNPC 2.0.11 + ColAndreas 1.5.0 + bcrypt 0.4.1, semua load sekali |
| Boot gamemode | log `[BOOT] LAST STAND siap. Zombie budget 190, zone 10.` |
| Spawn director | 59 zombie NPC aktif dalam 90 detik tanpa pemain |
| Navigasi AI | NPC berjalan ~30 m ke titik noise, lalu kembali IDLE (terukur dari log posisi) |
| MySQL hidup | MariaDB 10.11 via docker compose, 14 tabel, log `[MYSQL] tersambung ke laststand (10.11.19-MariaDB) dalam 5 ms` |
| Auth (Fase 3) | `git commit` modul `account/`: register bcrypt async, login bcrypt_verify, 2 slot karakter, save posisi/HP/infeksi saat disconnect |
| Uji beban 100 pemain | `bot_run.py --count 100`: 99/100 spawn, 90 zombie aktif, 0 assertion, 0 SIGABRT, tick director puncak 24 ms (bukti: docs/CODEBASE_STUDY.md §11h) |
| Belum terbukti | register/login lewat dialog butuh client nyata; soak 60 menit & p95 belum diukur |

Catatan runtime: `plugins/*.so` tidak masuk repo (lihat `docs/RUNTIME.md`), `ColAndreas.cadb` belum ada
sehingga LOS selalu lolos. `maxplayers` (300) wajib > `maxnpc` (200) **dan > pemain + NPC**: SA-MP
memakai satu pool slot untuk manusia dan NPC, jadi budget zombie mengikuti `Npc_SpawnRoom()`
(`modules/core/npc_budget.inc`), bukan angka `MAX_ZOMBIES`.

## Fase 0 — Tooling (½ hari)

Tugas:
1. Install `sampctl` (`curl -sL https://raw.githubusercontent.com/Southclaws/sampctl/master/install.sh | sh`).
2. `pawn.json` dengan versi **di-pin** (open.mp, YSI5, mysql, streamer, sscanf, fcnpc, colandreas, bcrypt) — jangan pakai `latest`.
3. `config.json` open.mp: nama server, max players 100, password staging, `rcon_password` dari env (jangan hardcode).
4. MySQL via docker compose (`mariadb:10.11`, port 3306, volume lokal) — mesin ini belum punya `mysql` client.
5. `.github/workflows/build.yml` dari skill `samp-gamemode-architecture`.
6. Jalankan `001_init.sql`.

Gate: `sampctl package build` exit 0, server start, `print` muncul di log.

**Subagent:** `samp-server-admin` (config & systemd), `samp-gamemode-architect` (skeleton).

---

## Fase 1 — Framework Inti (2–3 hari)

Tugas:
- `core/database.inc`: satu connection pool, wrapper `DB_ThreadedQuery` dengan callback `y_inline`. Tidak ada query sinkron.
- `core/logger.inc`: level + Discord webhook + rotasi file.
- `player/auth.inc`: register/login bcrypt, dialog validasi, rate limit 5 percobaan/menit per IP.
- `player/characters.inc`: 2 slot, pilih karakter, spawn terakhir.
- `interact/dispatcher.inc`: `CreateDynamicSphere` + `OnPlayerEnterDynamicArea` → prompt `[Y]`; fallback `/interact` untuk mobile.
- `world/zones.inc`: tabel zona (polygon sederhana via radius), fungsi `Zone_GetPlayerZone`, `Zone_IsPvP`.
- `world/daynight.inc`: siklus 18 menit (14 siang / 4 malam), `SetWorldTime` per 30 s.

Gate: daftar → login → pilih karakter → spawn di hub → masuk zona Idlewood → relog → posisi & uang kembali.

**Subagent:** `samp-pawn-wizard` (YSI), `database-reviewer` (index & query), `samp-gamemode-architect`.

---

## Fase 2 — Zombie Engine (4–6 hari, fase paling berisiko)

Tugas:
1. `core/spatial_hash.inc` + unit test logika sel (bisa diuji dengan command debug).
2. `zombie/zombie_core.inc`: pool `MAX_ZOMBIES 256`, `Zombie_Create/Destroy`, iterator.
3. `zombie/zombie_ai.inc`: LOD FULL/CHASE/IDLE, repath, stuck detection, attack dengan LOS.
4. `zombie/zombie_types.inc`: 7 tipe dari tabel DESIGN (HP, speed, damage, skin, loot).
5. `world/nodes.inc` + generator node graph per zona (simpan ke `scriptfiles/nodes/*.json`).
6. `zombie/spawn_director.inc`: populasi target per zona, spawn di luar 35 m & frustum, cooldown.
7. Command debug `/ztest <tipe> <jumlah>` (admin only).

Gate: 40 ambient + 150 horde, tick p95 < 20 ms, zombie tidak menembus dinding (uji 20 kali di gang sempit), tidak ada zombie diam di depan pemain.

**Subagent:** `samp-developer`, `samp-high-performance-tuning`, `performance-optimizer`, `samp-crashdetect-debugger` (jika crash AMX).

Risiko utama: FCNPC movement halus vs budget tick. Mitigasi: turunkan ambience dulu, naikkan hanya jika p95 masih < 15 ms.

---

## Fase 3 — Combat & Loot (3–4 hari)

Tugas:
- `config/items.inc`: registry item (id, model, berat, stack, kategori, durability).
- `inventory/inventory_core.inc`: 20 slot + bobot, `Inventory_Add/Remove/MoveTx`.
- `inventory/drop.inc`: dropped item dengan pickup player-specific.
- `combat/damage_pipeline.inc`: 7 langkah validasi (lihat ARCHITECTURE §6).
- `combat/noise.inc`: noise radius per senjata → tambah threat zona + trigger investigasi zombie terdekat via spatial hash.
- `combat/durability.inc`: hit melee, kondisi firearm, kecepatan tembak.
- `loot/node_registry.inc`: 120 node tier 1–2, respawn cycle dari DB.
- `loot/loot_tables.inc`: weighted table per tier, RNG **server-side saja**.

Gate: matikan 10 Walker dengan melee & 5 dengan pistol; loot masuk; relog item masih ada; dua pemain mengambil node yang sama tidak menghasilkan duplikat (uji dengan 2 client).

**Subagent:** `samp-pawn-wizard`, `database-reviewer`, `security-reviewer` (dupe & RNG).

---

## Fase 4 — Survivability (2 hari)

> **Status 25-11-18**: meja crafting kamp aktif (`/craft`, hotspot Y di 1666,-2320).
> 9 resep: Bandage, Medkit, Antibiotics, Blood Pack (butuh blueprint defib), Barricade Kit,
> Peluru Pistol/Rifle, Perbaiki alat (+40%), Perbaiki senjata di tangan (+40%).
> Semua bahan diambil lewat `Inventory_Consume` (ledger `craft`), gagal di tengah = refund otomatis.
> Stash 60 slot dan durabilitas item/senjata sudah tersimpan di MySQL.

Tugas: needs (hunger/thirst/stamina), infection meter + stage (40/80/100), Turned state 90 s, downed 45 s, revive channel (defib 6 s / bandage 12 s), medical bill saat mati.

Gate: simulasi 2 pemain — A downed, B revive dalam 45 s → berhasil; biarkan timer habis → mati + bill benar; infeksi 100% → jadi zombie dan bisa menyerang B; efek visual (skin + HUD merah) muncul.

**Subagent:** `samp-developer`, `samp-textdraw-fivem-master` (HUD infeksi), `a11y-contrast-enforcer` (kontras meter).

---

## Fase 5 — Hub & Ekonomi (3 hari)

Tugas: stash 60 slot (UI grid), crafting bench + 6 blueprint, klinik, pedagang ammo dengan stok terbatas, kontrak harian (3 slot, reset 00:00 UTC), konversi loot → scrap, tabel `item_ledger`.

Gate: ekstrak → stash → craft bandage → sembuhkan infeksi dengan antibiotics; kontrak harian selesai 1× lalu tidak bisa diulang (idempotency); cash tidak bisa negatif; dupe test 50× transfer bolak-balik tanpa selisih total.

**Subagent:** `samp-modern-jobs-architect`, `samp-textdraw-fivem-master`, `database-reviewer`.

---

## Fase 6 — Endgame Loop (4 hari)

Tugas: safehouse per karakter, 5 shelter dead zone, barricade (maks 400 objek global, HP, repair), Horde Night FSM (WARN→SPAWNING→ACTIVE→COOLDOWN), skill tree 5 cabang, XP & diminishing return, Quarantine Rank.

Gate: Horde Night otomatis di malam ke-3 tanpa admin; barricade hancur dalam ±20 s oleh 3 Brute; XP tidak naik saat membunuh di safe zone; skill poin tersimpan setelah relog.

**Subagent:** `samp-gamemode-architect`, `samp-security-anticheat` (anti-farm XP), `performance-optimizer` (400 objek).

---

## Fase 7 — Event & Sosial (3 hari)

Tugas: airdrop (25–35 menit, contested, marker global, 3 menit), blackout 20% saat malam, boss "The Butcher" 1×/jam, party 4 orang + shared XP, market hub antar pemain.

Gate: 4 pemain party ekstrak bersama → pembagian payout & XP sesuai kontribusi; airdrop tidak spawn di safe zone; boss drop masuk stash hanya untuk yang ekstrak.

**Subagent:** `samp-modern-jobs-architect`, `samp-creator-2026`.

---

## Fase 8 — Hardening & Scale (4–5 hari)

Tugas:
1. Semua matriks anti-cheat §10 aktif + webhook audit.
2. Load test: bot 100 koneksi (bisa pakai script Node `samp-query`/fake client) + 190 zombie.
3. Mobile pass: semua interaksi punya command fallback, target sentuh ≥ 44 px efektif, tidak ada keybind wajib.
4. Voice (opsional): sampvoice proximity + radio party (`samp-voice-spatial-audio`).
5. Crash & memory pass: 60 menit soak tanpa crash, `crashdetect` bersih.
6. Dokumentasi operator: cara restart, backup DB, tuning `zombies.inc`.

Gate: 100 pemain + 190 zombie, tick p95 < 20 ms, 0 crash 60 menit, 3 cheat simulasi (speed, wallbang, teleport) tertangkap.

**Subagent:** `samp-security-anticheat`, `samp-high-performance-tuning`, `samp-mobile-android-cef-ui`, `code-reviewer`.

---

## Pembagian Subagent per Disiplin

| Disiplin | Agent | Dipakai di fase |
|---|---|---|
| Arsitektur & struktur modul | `samp-gamemode-architect`, `samp-architecture-sage` | 0, 1, 6 |
| PAWN advanced & YSI | `samp-pawn-wizard`, `samp-developer` | 1–7 |
| Desain sistem & ekonomi | `samp-modern-jobs-architect`, `samp-creator-2026` | 5, 7 |
| UI/TextDraw | `samp-textdraw-fivem-master` | 1, 4, 5 |
| Keamanan & anti-cheat | `samp-security-anticheat` | 3, 6, 8 |
| Performa | `samp-high-performance-tuning`, `performance-optimizer` | 2, 6, 8 |
| Crash triage | `samp-crashdetect-debugger` | 2, 8 |
| Mobile | `samp-mobile-android-cef-ui` | 8 |
| Review kode | `code-reviewer`, `security-reviewer` | akhir tiap fase |

---

## Risk Register

| Risiko | Dampak | Prob. | Mitigasi |
|---|---|---|---|
| FCNPC 190 zombie bikin tick spike | Tinggi | Tinggi | LOD agresif, slice 5 grup, budget per tick, turunkan ambient duluan |
| Pathfinding node graph buruk | Tinggi | Sedang | Generate + prune ColAndreas, fallback CHASE wajib, uji di 5 lokasi sempit |
| Dupe item saat transfer | Tinggi | Sedang | `FOR UPDATE` + `item_ledger`, uji 50× bolak-balik |
| Pemain mobile tidak bisa pakai keybind | Sedang | Tinggi | Semua interaksi punya command fallback sejak fase 1, bukan ditambal di akhir |
| Zombie menembus dinding (bug klasik) | Sedang | Tinggi | LOS ColAndreas di attack + uji gang sempit |
| TextDraw 256 limit jebol | Sedang | Sedang | Global TD untuk frame statis, `CloseActiveUI`, headroom 60 |
| Server kosong di minggu 1 (retensi) | Tinggi (bisnis) | Tinggi | Onboarding 5 menit, kontrak harian, Horde Night tiap ~54 menit nyata |
| Scope creep (custom map, CEF, faction RP) | Tinggi | Tinggi | Semua di luar 9 fase ini = **tidak dikerjakan** sampai fase 8 lulus |

---

## Definition of Done (fase apa pun)

1. `sampctl package build` exit 0, tanpa warning baru.
2. Checklist `tests/fase-N.md` dijalankan manual dan ditandai.
3. Tidak ada query non-threaded baru (`grep -n "mysql_query\|mysql_tquery"` di modul baru).
4. Tidak ada `for (i = 0; i < MAX_PLAYERS` baru.
5. Review oleh `code-reviewer`; temuan tingkat tinggi = blocker.
6. Invariant baru ditulis ke `MEMORY.md`.

## Base Building (selesai, v1)

- [x] Skema `bases.tier/pos_*` + `base_structures` (FK CASCADE, diuji hapus akun)
- [x] 6 struktur: Wall, Gate, Crate, Bench, Bed, Turret
- [x] Validasi penempatan server-side (tanah ColAndreas, radius petak, jarak, larangan hub)
- [x] Zombie merusak struktur, HP disimpan bertahap, hancur = baris DB hilang
- [x] Crate menambah kapasitas stash, Bench membuka `/craft`, Bed memindahkan respawn
- [x] `/build`, `/base`, `/upgrade`, `/bongkar`, `/fix`, entri `/menu`
- [x] Raid terarah ke base (horde memilih base ber-heat tertinggi) — `zombie/horde.inc:38` sudah memilih base ber-heat tertinggi; terverifikasi selftest.
- [x] Base party/multi-pemain + izin gate: `024_base_members.sql` + `world/base_members.inc` (role member/officer/owner, `/base add|officer|remove|members`), gate `STRUCT_GATE` dibuka tombol **Y** oleh owner/officer (`BaseBuild_ToggleGate`), auto-close 8 detik, selftest `izin gate base` 45/45
- [x] Dekorasi model tambahan: `world/decor.inc` — 6 prop + 3 papan tanda memakai model yang sudah terbukti (19447/1427/19477), `/decor` (daftar) dan `/decor tp <n>` (pindah ke dekorasi untuk dilihat di client); selftest `dekorasi kamp` (9 dekorasi, indeks valid, 0 duplikat)

## Fondasi telemetri & progression (selesai)

- [x] `kill_log` menyimpan jarak tembak nyata (dihitung sebelum mayat dibersihkan)
- [x] `/stats`: agregasi zona, senjata, tipe zombie per karakter (satu query UNION)
- [x] Respec skill: level → poin, biaya Scrap 3, dicatat `item_ledger` (`respec`)
- [x] Raid horde terarah ke base ber-heat tertinggi + `/raidbase` untuk uji
- [x] Indeks `kill_log(zone_id, created_at)`; indeks lain sudah ada sejak migrasi awal
- [x] Self-test otomatis 8 dtk setelah boot (19 pemeriksaan, PASS/FAIL di log)
- [x] Leaderboard musiman lintas karakter — `database/migrations/023_season_archive.sql` + `scripts/season-rollover.sh` (agregasi 4 kategori, idempoten lewat `ON DUPLICATE KEY UPDATE`), kategori `ARSIP` di `/top`. Terbukti: 2 kill Oktober → `ARSIP 2026-10 ZZ_Arsip 42`, jalankan dua kali tetap 4 baris. Catatan: `uang`/`jam`/`rank` adalah snapshot saat arsip, bukan selisih bulanan.
- [x] Laporan mingguan otomatis — `scripts/weekly-report.sh [hari]` (read-only: populasi, kill per zona, senjata dominan, top pembunuh, ekonomi, ban/flag). Dijadwalkan Senin 04:20 oleh `scripts/install-schedule.sh`. Diuji langsung: exit 0, angka nyata dari DB.

## Survival tubuh & kendaraan (selesai)

- [x] Hunger, thirst, bladder di state + HUD + peringatan + damage ekstrem
- [x] Persistensi `characters.hunger/thirst/bladder` (load/save/autosave)
- [x] Makan/minum memengaruhi meter; `/kencing` di mobile dan desktop
- [x] Kendaraan liar spawn otomatis per lobby (maks 5, raycast tanah, jarak aman)
- [x] Bahan bakar liter per model (tangki + konsumsi realistis, skala permainan)
- [x] `/isi` hanya di pom kamp (cash $5/L), Jerrycan (item, di mana saja)
- [x] Garasi 4 kendaraan: beli, keluarkan, simpan, asuransi saat hancur
- [x] Kepemilikan kunci: pemilik via `/kunci`, pemain lain via `/lockpick`
- [x] Bensin + kondisi mobil tersimpan di `owned_vehicles` dan bertahan setelah relog
- [x] Lockpick: item, craft, pedagang, loot langka; 65% berhasil, terkunci tanpa bypass
- [x] Ekstraksi berbasis sampel: Specimen 8%/35%, min 3, $120/sampel + $100,
      cooldown 90 s/lobby, sampel tidak bisa di-stash, tekanan Director +4
- [x] Repair kendaraan: `/perbaiki` (Repair Kit + Scrap 3), mesin mati saat <350 HP
- [x] Siphon bensin: `/sedot` ke jerrycan (20 L, durability = isi liter)
- [x] `/isi kan` mengisi ulang jerrycan di pom ($5/L)
- [x] Luka terbuka 0–3 stack, perdarahan tiap 5 detik, bandage/medkit, persist DB
- [x] Generator base: BBM liter, hanya malam, turret tanpa daya berhenti menembak
- [x] Escort trader FCNPC: satu misi per lobby, hadiah cash/scrap/medkit/XP
- [x] Peti dunia langsung 3 saat spawn, maks 8/lobby, ikon peta kuning
- [x] Budget NPC dinamis per pemain + FCNPC sync adaptif 80/100 ms (anti stutter animasi)
- [x] Pathfinding LOS langsung ≤30 m + repath hanya saat target bergerak ≥1,5 m; LOD 45/90 m
- [x] Profiler tick `[PERF]` + `/diag` ms per sistem; autosave stagger 5 karakter/10 dtk; MySQL pool 4
- [x] Self-test 26/26 (termasuk validasi pool kendaraan, item, dan kolom baru)

## Onboarding (selesai)

- [x] Migrasi `013_tutorial.sql`: kolom `characters.tutorial_step` + `tutorial_count`
- [x] Modul `player/tutorial.inc`: 5 langkah persisten (bunuh 3 zombie → loot → craft → 3 sampel → ekstraksi)
- [x] Hook di `Zombie_AwardKill`, `Loot_TakeNearest`, `OnPlayerPickUpDynamicPickup`, `CraftUI_Craft`, `Extraction_Try`
- [x] Reward tiap langkah (Bandage/Scrap/Medkit/Antibiotics) + final $300 + 5 scrap
- [x] `/misi` menampilkan target + progres; intro otomatis saat karakter baru dimuat
- [x] Progres tersimpan ke MySQL di setiap kenaikan langkah (tahan reconnect/crash)
- [x] Self-test 27/27 (cek target/label langkah onboarding)

## Core loop (selesai)

- [x] Makam pemain: tas jatuh utuh, 16 makam, 10 menit, eksklusif pemilik 2 menit
- [x] Makam persisten di MySQL (`graves` + `grave_items`), dimuat saat boot
- [x] Filter world + lobby pada makam (shelter tidak bocor ke dunia utama)
- [x] Kontrak harian 3 slot, deterministik per `(char_id, day_id)`, anti-farming di kamp
- [x] Klaim hadiah cash + item + XP, progres disimpan saat klaim/autosave/shutdown
- [x] `/kontrak`, `/geledah`, menu pusat baris 13, self-test menghitung tabel baru

## Loot 2.0 (selesai)

- [x] Tabel bobot per tipe zombie + jumlah roll berbeda per tipe (Walker bisa kosong)
- [x] Bonus langka terpisah: 8% dasar, +tier zona, +4% malam, maks 25%
- [x] Peti dunia acak: maks 6 per lobby, 30 dtk, jauh minimal 25 m dari pemain
- [x] Jitter posisi drop supaya tidak menumpuk; purge drop tertua saat meja penuh
- [x] `/lootnode` (admin) + cek tabel loot di self-test (21 pemeriksaan)
- [x] Log aplikasi benar-benar ditulis ke `scriptfiles/logs/laststand.log`
- [x] Loot per tier zona: slot bonus Medkit/Rifle ammo (tier 2) dan Blood pack/Blueprint (tier 3)
- [x] Peti dunia punya objek kayu + ikon koper + label 60 m untuk mobile
- [x] Mayat Brute/Stalker: dialog `/geledah` (maks 24 mayat, 5 menit, per lobby)
- [x] Bobot rifle ammo diturunkan ~25% di semua tabel
- [x] 7 baris `anticheat_flags` false positive dibersihkan
- [x] Loot khusus misi/event (buku catatan, kunci, dokumen) — `ITEM_DOCUMENT=29`/`ITEM_NOTEBOOK=31` ada di `config/items.inc:57-59`, drop di `loot/drops.inc:524,546`, dipakai `progression/contracts.inc:363`.
- [x] Berat tas memengaruhi kecepatan lari — `Bag_SpeedScale`/`Bag_SpeedTick` di `inventory/bag.inc` (maks 22% lebih lambat), dipanggil dari `Player_Tick` 500 ms di `main.pwn`, BUKAN `OnPlayerUpdate`. Selftest `berat tas vs kecepatan`.

## Sub-lobby (selesai)

- [x] `config/lobbies.inc`: 3 lobby (Quarantine/Outbreak/Nightfall), world 100/200/300
- [x] `/lobby` + baris menu; hanya di kamp, tersimpan di `characters.lobby_id`
- [x] Zombie, loot, kendaraan, barricade, struktur base hidup di world lobby pemilik
- [x] Heat horde dan raid base per lobby; chat/toast tidak bocor antar lobby
- [x] Shelter base per lobby: `SHELTER_VW_BASE + lobby * 400 + base % 400`
- [x] Self-test 20 pemeriksaan (tambah cek registry lobby)
- [x] Nightfall (lobby 2) diaktifkan setelah populasi cukup — `Lobby_AutoTick()` (ambang 20 pemain, histeresis 3/10 tick) dipanggil dari `Director_Report` tiap 60 s; malam permanen per pemain via `Lobby_ApplyClock`; toggle manual `/nightfall`. Selftest `nightfall gate`.
- [x] Migrasi lintas lobby untuk base yang sudah ada: `BaseBuild_MigrateLobby` dipanggil dari `Lobby_Set` — `base_structures` tetap, visual dimuat ulang di world lobby baru, `bases.lobby_id` ikut diperbarui

## Multi-server (selesai, alternatif)

- [x] `scripts/instance.sh`: init/start/stop/restart/status/logs/list per instans
- [x] Port + database + hostname + rcon terpisah per instans; `main.amx` dan `cadb` bersama
- [x] Migrasi otomatis membuat database baru (root hanya untuk `CREATE DATABASE`)
- [x] Identitas instans di log: `[BOOT] instans: <nama> | port <port>`
- [x] Dua instans berjalan bersamaan, self-test 19/19 PASS di keduanya
- [x] Akun lintas server: `026_server_instance.sql` (`characters.server_id` + index `(server_id, account_id)`), `core/instance.inc` (`Server_InstanceId()` 1..9 dari `server_id` di server.cfg); daftar/muat/buat karakter difilter, `accounts` tetap bersama; bukti SQL isolasi lintas instance
- [x] Antrean masuk saat penuh: `core/slotqueue.inc` — `player_cap` (default 180) < maxplayers, FIFO 5 detik, `/masuk`+`/daftar` diblokir `Queue_BlockAuth` selama menunggu, timeout 10 menit dengan pesan jelas; selftest batas `179/180/181 = 0/1/1`. Load balancer DNS/proxy UDP tetap pekerjaan infra, bukan PAWN

## Operasi & pemulihan (selesai)

- [x] `scripts/backup.sh`: dump `laststand` + `laststand_s2`, salin `server.cfg`/`mysql.ini` (root/s1/s2), `SHA256SUMS`, `MANIFEST` (+ sha256 `main.amx`), retensi `BACKUP_KEEP` (default 7)
- [x] `scripts/restore.sh`: backup terbaru atau folder tertentu, verifikasi checksum, konfirmasi `RESTORE`, snapshot `pre_restore_*` otomatis, laporan jumlah tabel per DB
- [x] Verifikasi: gzip -t lolos, paritas 20 tabel dump vs 20 tabel live di kedua DB
- [x] Jadwal harian via cron/systemd timer — `scripts/install-schedule.sh` (`--print` default aman, `--install` menulis crontab pengguna, `--remove`). Backup 04:15, laporan mingguan Senin 04:20.

## Loot misi/dokumen (selesai)

- [x] Item baru: `ITEM_DOCUMENT` (29), `ITEM_WAREHOUSE_KEY` (30), `ITEM_NOTEBOOK` (31); `ITEM_COUNT` 32
- [x] Slot kontrak ke-4 `CONTRACT_KIND_DOCS`: target 2-4 dokumen, hadiah `$260+45/target` + 1 Kunci Gudang
- [x] Progres dokumen otomatis saat mengambil loot (dua jalur grant) dan lewat pakai Buku Catatan
- [x] Kunci Gudang: spawn peti rare 6 m dari pemain, hanya di luar kamp, refund otomatis kalau peti gagal muncul
- [x] Vendor: Dokumen $180, Buku Catatan $90 (daftar jual 7 → 9)
- [x] Dokumen lore tertulis: `content/lore.inc` — 12 teks, `/dokumen` (alias `/lore`), item `ITEM_DOCUMENT` memakai dialog teks; selftest `dokumen lore`

## Raid base terarah (selesai)

- [x] Heat **per base** (`BaseHeat[lobby][slot]` + guard `BaseHeatChar`), hanya noise dalam 26 m petak base yang menaikkan heat, decay 0.94/tick
- [x] Cooldown raid **per lobby** (bukan satu tick global yang memblok lobby lain)
- [x] Zombie raid di-anchor ke petak base: chase dibatasi 38 m, kembali ke petak bila kosong, dinding digerus `BaseBuild_ZombieTick`
- [x] Hadiah bertahan: $420 + 35 XP saat semua raider mati (`Horde_CountRaidZombies` hanya menghitung zombie ber-anchor), timeout 180 s
- [x] `/raidbase` admin ikut memakai anchor + jalur hadiah yang sama
- [x] Banner "raid" khusus di HUD: `HudRaid` (PlayerTextDraw y=16, di atas bar boss y=30) menampilkan `RAID KAMP <n> ZOMBIE <m:ss>`. Jumlah zombie dibaca dari cache `HordeRaidCount` yang disegarkan `Horde_Tick` tiap 5 dtk — jalur HUD 500 ms tidak memindai `ZombiePool`. Guard lobby invalid diuji selftest (`banner raid HUD`), total cek jadi 37.

## Party co-op v1 (selesai)

- [x] `player/party.inc`: ketua + maks 4 anggota, undangan 30 s dengan dialog Terima/Tolak, auto-bubar saat ketua keluar/disconnect
- [x] Friendly fire mati di `Damage_Apply` (bukan hanya `OnPlayerGiveDamage`) sehingga ledakan/kendaraan juga tidak melukai anggota
- [x] XP dibagi 20% ke anggota hidup dalam 60 m di lobby sama, guard `PartyXpGuard` mencegah rekursi
- [x] Panel `/party`: HP, jarak, status TUMBANG; ketua bisa menendang dari panel (pakai snapshot baris)
- [x] Chat party `/p <pesan>` + perintah `/invite` `/accept` `/leave` `/kick` (semua ada fallback "terdekat" untuk mobile)
- [x] Selftest baru: `konsistensi party` (29/29 PASS)
- [x] Stash/base bersama: diputuskan memakai `party_stash` (sudah ada sejak `017_party_stash.sql`) sebagai kontainer bersama party; `StashContents` tetap per-karakter untuk stash pribadi — tidak ada tabel baru, tidak ada stub
- [x] Marker anggota di HUD/radar — `Party_ApplyMarkersFor`/`Party_RefreshMarkers` di `player/party.inc` memakai `SetPlayerMarkerForPlayer` (warna hijau party, dikembalikan ke `GetPlayerColor` saat keluar). Tanpa blip tambahan, jadi 0 slot terpakai.


## NPC rekrutan / companion (selesai)

- [x] Migrasi `014_companion.sql`: tabel `companions` (UNIQUE `char_id`) — 1 rekrutan per karakter, persist lintas relog
- [x] `world/companion.inc`: FCNPC netral, ikut pemilik, menembak zombie terdekat ≤26 m (damage 13 / 900 ms, tanpa atribusi kill), suara tembakan memancing horde
- [x] Zombie mencakar rekrutan (simulasi melee 6 HP/2 s per zombie menempel); tumbang = bendera `CompDowned`, bukan kematian NPC
- [x] `/teman` panel: ikut / jaga / tolong (1 perban) / bubarkan; `/teman ikut|tunggu|revive|buang`
- [x] Perekrut statis di kamp (actor + label `{Y}`), biaya $600 + 2 makanan kaleng + 1 air mineral, refund bekal kalau gagal
- [x] Rekrutan hilang permanen kalau tidak ditolong dalam 3 menit
- [x] `maxnpc` 190 → 210 supaya rekrutan tetap bisa spawn saat zombie penuh
- [x] Selftest baru: `anggaran NPC rekrutan` (30/30 PASS)
- [x] Peralatan/upgrade rekrutan: senjata per tier (24 → 31 → 34), armour (0/25/50) lewat `Companion_ApplyGear`, dan rekrutan membawakan loot di radius 4 m (`Loot_TakeNearPoint`, maks 1 per 3 detik); selftest `perlengkapan rekrutan naik per tier`
- [x] Interaksi tombol Y langsung ke perekrut — `Companion_TryRecruitInteract` dipanggil dari `interact/dispatcher.inc` sebelum hotspot, jadi `/rekrut` tetap ada sebagai cadangan.


## Boss malam / Abomination (selesai)

- [x] `ZT_BOSS` = tipe ke-8 di `config/zombies.inc` (skin 8, 2400 HP, damage 45, radius 1.8), `minTier 99` → tidak pernah muncul dari spawn rutin
- [x] `Director_BossTick` dipanggil `Director_NightWatch`: hanya malam, maks 1 boss/lobi, cooldown 15 menit, butuh pemain hidup di luar hub, spawn 30-55 m dari pemain
- [x] Tabel loot boss sendiri (10 slot penuh) + jaminan drop `Specimen` (chance 100)
- [x] Hadiah kill boss sekali: $250 + 60 XP + 3 scrap + toast "ABOMINATION MATI"; `Director_OnBossKilled` membersihkan tracking per lobi
- [x] Selftest baru: `boss malam` (31/31 PASS)
- [x] Bar HP boss di HUD — `HudBoss` (PlayerTextDraw, bukan global) di `ui/hud.inc`, membaca `Director_BossStatus`; ikut gate lebar 640 px.
- [x] Fase boss — `Director_BossPhaseTwo` (`spawn_director.inc:454`) menambah 5 runner sekali saat HP < 50% (`ZOMBIE_BOSS_PHASE2_PERCENT`).


## Sistem role admin (selesai)

- [x] Migrasi `015_roles.sql`: `accounts.admin_level` (0/1/2) + index
- [x] `admin/roles.inc`: level dimuat saat login, pemilik (`ferzdevz`) otomatis superadmin + persist, reset saat disconnect
- [x] Semua gate dev/admin pindah dari native RCON `IsPlayerAdmin` ke `IsServerAdmin` (level DB)
- [x] `DEV_MODE` 1 → 0: `/heal /hurt /infect /zspawn /horde /raidbase /lootnode /event /diag /selftest` tidak lagi terbuka untuk pemain biasa
- [x] `/admin` (info level + daftar perintah) dan `/setadmin <id> <0/1/2>` (superadmin, tidak bisa mengubah level sendiri)
- [x] Selftest memverifikasi `accounts.admin_level` + tabel `companions` (31/31 PASS)
- [x] `/ban` + `/unban` memakai tabel `bans` — sudah: `admin/moderation.inc:60` INSERT, `:151` UPDATE expires_at, `:227` daftar ban aktif.
- [x] Audit log admin ke tabel sendiri — `database/migrations/022_admin_actions.sql` + `modules/admin/audit.inc` (`AdminAudit_Log`, `/adminlog [n]`). Sudah dipanggil dari ban/unban/mute/warn.


## Moderasi ban/unban (selesai)

- [x] `admin/moderation.inc`: `/ban <id> <menit|0> [alasan]`, `/unban <nama>`, `/bans`
- [x] Tabel `bans` (dari migrasi 001) akhirnya dipakai: `account_id`, `reason`, `admin_name`, `expires_at` (0 = permanen), `created_at`
- [x] `accounts.is_banned` tetap jadi jalur cepat login; detail ban dibaca dari `bans`
- [x] Auto-unban: login dengan masa blokir habis → flag dibersihkan otomatis, riwayat tetap tersimpan
- [x] Aturan: admin tidak bisa mem-ban admin lain (superadmin bisa), tidak bisa ban diri sendiri
- [x] `maxnpc` 210 → 200 (FCNPC menolak `maxnpc > maxplayers`; 190 zombie + 9 rekrutan + 1 trader = 200 pas)
- [x] Selftest: tabel `bans` ikut dihitung (31/31 PASS, `errors.log` 0 byte)
- [x] `/mute` + `/unmute` + `/warn` — `admin/moderation.inc` (`Moderation_Mute/Unmute/Warn`), chat diblokir di `OnPlayerText` dengan sisa menit; 3 peringatan → mute 15 menit. Selftest `mute dan warn`.
- [x] Audit log admin ke tabel sendiri — sama dengan baris di atas: tabel `admin_actions` (migrasi 022), detail + target_account + admin_name, tetap dobel ke file log bila DB mati.


## Batch lanjutan (selesai)

- [x] **Cron backup harian** `0 4 * * *` → `backups/cron.log`
- [x] **`/top` papan peringkat**: kill zombie (`kill_log` victim_kind 2), uang, jam main, rank point — satu query UNION ALL, 5 besar per kategori
- [x] **Tier rekrutan** (`016_companion_tier.sql`): `/teman upgrade`, tier 1 ($400 + 6 kain + 2 elektronik) HP 160 dmg 19, tier 2 ($900 + 1 repair kit + 4 elektronik + 10 amunisi rifle) HP 220 dmg 26; validasi semua bahan dulu, refund kalau gagal
- [x] **Bar HP boss + fase 2**: `HudBoss` PlayerTextDraw, `Director_BossStatus`, `ZOMBIE_BOSS_PHASE2_PERCENT 50`, `ZOMBIE_BOSS_PHASE2_ADDS 5`, flag `BossEnraged` reset saat spawn/mati
- [x] **Peti bersama party** (`017_party_stash.sql` + `player/party_stash.inc`): `/peti`, `/peti taruh`, 40 slot, ambil/taruh seluruh stack, ledger `party_stash_take`/`party_stash_put`
- [x] Selftest 31/31 (tabel `party_stash`, kolom `companions.tier` ikut diverifikasi)
- [x] Peti party: pindah sebagian — dialog ambil/simpan sudah menawarkan 1 / 5 / semua (`PartyStash_Take`/`PartyStash_Put` dengan `requestedAmount`)
- [x] Peti party: izin peringkat + riwayat: hanya petugas gudang (`/gudang <id>`, ditunjuk pemimpin) yang boleh **menaruh**; setiap take/put berhasil dicatat ke `party_stash_log` (`025_party_stash_log.sql`) dan dibaca `/gudang`


## Perkakas superadmin (selesai)

- [x] `admin/admin_state.inc`: flag `AdminGod` di-include SEBELUM `damage_pipeline` agar kebal memblokir semua damage (`Admin_IsGod` di cabang `DAMAGE_KIND_PLAYER`)
- [x] `admin/superadmin.inc`: 15 command + helper ber-guard
- [x] `admin/panel.inc`: panel dialog penuh `/sa` tanpa mengetik command — 7 kategori, 13 dialog id (3501-3513), snapshot baris (player/item/ban), jumlah dari preset, aksi langsung memanggil helper ber-guard yang sama dengan command
- [x] Spawn: `/spawnz walker|runner|crawler|brute|bloater|screamer|stalker|boss [1-30]`, `/zclear` per lobi
- [x] Item: `/giveitem <id|nama> [n]`, `/give <playerid> <id|nama> [n]`, `/itemlist [halaman]` — selalu lewat `Inventory_Grant` + ledger `admin_grant`
- [x] Stat: `/setcash` (cap $10 jt), `/setxp`, `/setskill` (cap 99), `/max` (HP/armour/kebutuhan/infeksi/luka)
- [x] Dunia: `/sethour 0-23` (`World_SetHour`), event lewat panel
- [x] Navigasi: `/tppos`, `/goto`, `/gethere`, `/freeze` — setiap teleport memanggil `AC_ClearSample`
- [x] Panel Teleport (dialog 3514): 6 titik populer + base sendiri + isi bensin/perbaiki kendaraan terdekat (repair admin menyuntik 1 Toolbox lalu langsung terpakai → saldo item nol)
- [x] Semua aksi level >= superadmin (kecuali `/max` dan `/freeze` yang cukup admin) + `Log_Format` berisi account id
- [x] `/weather <id>` + `/weather auto` + `/setfuel <liter>` — `World_SetWeather` (`world/daynight.inc`) dengan override yang tidak ditimpa `World_Tick`; selftest `override cuaca`.
- [x] Audit log tabel sendiri (`admin_actions`) — migrasi 022 diterapkan; selftest `tabel database` sekarang memverifikasi tabel ini ada.

## Pembuatan karakter (selesai)

- [x] Wizard 8 langkah: nama → gender → umur → ras → asal → pekerjaan → keahlian → penampilan → ringkasan.
- [x] Efek nyata per pilihan: uang awal, HP awal, XP, poin skill, kit item pekerjaan (lewat `Inventory_Grant`, alasan `char_start`).
- [x] Kolom profil di `characters` (migrasi `018_character_profile.sql`) + validasi selftest kolom `occupation`.
- [x] `/profil` untuk melihat identitas karakter.
- [x] Pemain tanpa karakter langsung masuk wizard (tidak ada lagi daftar kosong).

## Operasional & observability (sesi 26-11-23 lanjutan)

- [x] `scripts/healthcheck.sh` — cek kesehatan lewat protokol query SA-MP (UDP), mode tabel + `--json`, exit 1 bila ada instance tidak menjawab
- [x] Supervisor systemd untuk semua instance: `scripts/systemd/laststand-s1.service`, `laststand-s2.service`, `laststand.target`, installer `scripts/install-systemd.sh` (menggantikan unit usang `/home/boxd/...` yang hanya ada untuk s1); teruji `kill -9` → `NRestarts=1`, healthcheck tetap exit 0
- [x] Log kematian pemain: migrasi `027_deaths.sql` + `player/deaths.inc` — `OnPlayerDeath` tidak lagi membuang `killerid`/`reason`, klasifikasi `pemain/lingkungan/zombie`, `/kematian` (3 terakhir), retensi 90 hari; selftest `log kematian: klasifikasi penyebab` + `format waktu` (50/50)
- [x] Kunci unik karakter per instance: `028_character_keys_per_server.sql` (`uq_characters_server_name`, `uq_characters_server_slot`) + `029_drop_redundant_server_index.sql`; bukti nama+slot 0 kini boleh sama di s1 dan s2, EXPLAIN daftar karakter tetap `rows: 1`
- [x] Pratinjau pembangunan (`world/build_edit.inc`): ghost object + panel POSISI/ARAH/JARAK/NAIK, material dipotong saat `/pasang`, tombol ALT/C/Y/H + `/geser` `/putar` `/tinggi` `/pasang` `/batal` untuk Android
- [x] Perbaikan "struktur bergeser": penanda muat per **base** (`BaseBuild_IsLoaded`/`MarkLoaded`/`ForgetLoaded`) + pagar duplikat di `BaseBuild_OnRowsLoaded`; selftest `muat struktur per base` (52/52)
- [x] HUD: nominal uang dihapus, SCRAP/SAMPEL diberi kotak latar gelap, BBM tampil untuk pengemudi **dan** penumpang, plus baris GENSET saat berdiri <= 12 m dari generator sendiri

