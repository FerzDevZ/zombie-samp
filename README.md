# LAST STAND: San Andreas Quarantine

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Build](https://github.com/FerzDevZ/zombie-samp/actions/workflows/build.yml/badge.svg)](https://github.com/FerzDevZ/zombie-samp/actions/workflows/build.yml)
[![Platform](https://img.shields.io/badge/platform-SA--MP%200.3.7%20%2B%20Android-blue.svg)](#stack-yang-dipilih-dan-alasannya)

Server SA-MP / open.mp **zombie survival co-op** — ekstraksi loot, horde director, infeksi, dan base defense.
Bukan zombie deathmatch. Bukan roleplay. Loop-nya survival ekstraksi 20 menit yang bisa dimainkan solo atau party 4 orang.

## Kenapa server ini beda

| Server zombie SA-MP pada umumnya | Server ini |
|---|---|
| Zombie muncul di `/zombie` lalu ngejar terus | **Horde Director**: noise, waktu malam, dan populasi loot menentukan kapan horde datang |
| Tembak terus, ammo tak terbatas | **Noise economy**: tiap tembakan menarik zombie dari radius tertentu. Melee senyap = aman, M4 = bunuh diri |
| Mati → respawn → ulang | **Ekstraksi**: masuk dead zone, loot, keluar hidup-hidup. Mati = kehilangan carrier loot, stash di hub aman |
| Tidak ada progres | Skill tree 5 cabang + Quarantine Rank + blueprint crafting |
| Zombie = skin walker biasa | 7 tipe zombie dengan perilaku, resistensi, dan loot berbeda |

## Dokumen

| File | Isi |
|---|---|
| [docs/INSTALL.md](docs/INSTALL.md) | **Panduan lengkap**: prasyarat, database, build, multi-instans, supervisor 24/7, backup/restore, soak test, troubleshooting, checklist go-live |
| [docs/COMMANDS.md](docs/COMMANDS.md) | Semua perintah pemain/admin per kategori + contoh alur pemain baru + aturan UI mobile |
| [DESIGN.md](DESIGN.md) | Game design lengkap: pilar, loop, peta, roster zombie, senjata, infeksi, ekonomi, event, UI |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Blueprint teknis: stack, struktur modul, AI zombie, tick budget, DB, anti-cheat, TextDraw budget |
| [ROADMAP.md](ROADMAP.md) | 9 fase implementasi + gate verifikasi + pembagian subagent + risk register |
| [docs/PLAN_NEXT.md](docs/PLAN_NEXT.md) | Rencana prioritas + hasil pengukuran memori/CPU terakhir (angka nyata, bukan klaim) |
| [docs/PLAN_GO_LIVE.md](docs/PLAN_GO_LIVE.md) | Fase A–E menuju produksi + tabel risiko |
| [docs/RANCANGAN_2026.md](docs/RANCANGAN_2026.md) | Rancangan fitur lanjutan |
| [docs/CODEBASE_STUDY.md](docs/CODEBASE_STUDY.md) | Peta kode & alasan desain per modul |
| [database/migrations/001_init.sql](database/migrations/001_init.sql) | Skema MySQL awal (21 migrasi total) |
| [MEMORY.md](MEMORY.md) | Post-mortem & invariant yang sudah disepakati (baca sebelum mengubah sistem inti) |

## Mulai cepat (5 menit)

```bash
git clone https://github.com/FerzDevZ/zombie-samp.git && cd zombie-samp
# dependency, plugin .so, samp03svr, dan data ColAndreas sudah ikut di repo
docker compose up -d                        # MariaDB 10.11, migrasi otomatis
cp mysql.ini.example mysql.ini && cp mysql.ini scriptfiles/mysql.ini
sampctl build                               # 0 error (1 warning bawaan ColAndreas)
./scripts/instance.sh init s1 && ./scripts/instance.sh start s1
tail -f runtime/s1/server_log.txt           # tunggu [SELFTEST] 36/36 PASS
```

Detail lengkap (termasuk ColAndreas 54 MB, supervisor 24/7, backup, soak 100 bot)
ada di [docs/INSTALL.md](docs/INSTALL.md).

## Angka terukur (bukan klaim)

| Metrik | Nilai | Cara ukur |
|---|---|---|
| Boot | 30–35 detik | log server |
| RSS idle | ~498 MB | `ps -o rss` |
| Biaya per pemain | 0,58–0,80 MB | `tools/loadtest/mem_probe.sh` |
| 100 bot tersebar 8 zona + 96 zombie | RSS 498 → 552 MB **datar** | `./scripts/soak.sh 10 100 s1` |
| p95 tick (zombie/director/horde/player/hud) | ≤ 3 ms | `[PERF] tick` tiap 60 dtk |
| Selftest internal | 36/36 PASS | `/selftest` / log boot |

## Stack yang dipilih (dan alasannya)

- **open.mp** (bukan SA-MP 0.3.7) — komponen native, config JSON, crash handling lebih baik, tetap kompatibel client SA-MP termasuk **SA-MP Mobile Android**.
- **YSI 5** (`y_hooks`, `y_iterate`, `y_timers`, `y_commands`) + **Incognito Streamer** + **BlueG MySQL R41-4** (100% threaded) + **FCNPC** (zombie) + **ColAndreas** (validasi raycast) + **bcrypt**.
- **sampctl** untuk dependency & compile gate CI.

Alternatif yang ditolak: SA-MP 0.3.DL (custom model bagus tapi client mobile tidak dukung), plugin zombie pihak ketiga (tidak ada yang bisa diatur sedetail AI custom), CEF (belum ada di Android).

> **Catatan runtime (26-11-19):** yang terpasang dan terverifikasi saat ini adalah **SA-MP 0.3.7** (`samp03svr`, include `<a_samp>`), bukan open.mp — migrasi open.mp masih rencana. Semua fitur di atas berjalan di 0.3.7 + plugin yang disebut. Kunci admin runtime ada di `scriptfiles/admin_keys.cfg` (tidak masuk repo; rotasi tanpa compile).

## Status

Playable, bukan scaffold. Yang sudah jalan dan terverifikasi:

| Sistem | Bukti |
|---|---|
| Auth bcrypt + karakter + autosave 2 menit | login/registrasi dialog + `/login` mobile, `characters` tersimpan |
| Pembuatan karakter 8 langkah | nama, gender, umur, ras, asal, pekerjaan, keahlian, penampilan → ringkasan sebelum dibuat; tiap pilihan memberi efek nyata (uang/HP/XP/poin skill/kit item) dan tersimpan di kolom `characters.gender…appearance` |
| Login & daftar lewat chat | `/daftar <password>`, `/masuk <password>`, `/akun`, `/nama <namakarakter>`, `/gantipass <lama> <baru>`, `/pulihkan <password> [kunci pemilik]` (akun mana pun kalau kuncinya benar). Dialog input tidak dipakai karena klien mobile sering mengirim teks kosong |
| Login admin pakai kunci | `/superadmin <kunci>` = level superadmin, `/admin <kunci>` = level admin, untuk akun mana pun; level tetap tersimpan di `accounts.admin_level` |
| Tas bertingkat | Tanpa tas 30 kg, Tas Kecil 40 kg, Tas Harian 50 kg, Tas Gunung 62 kg, Tas Militer 75 kg; model menempel di punggung (bone 1, model 1550) dan skala naik per tier; beban ≥90% mempercepat haus/lapar 35% |
| Inventory 20 slot + stash 60→120 slot | satu `REPLACE` per tabel, uji SQL lolos, `item_ledger` sebagai audit; stash dibatasi berat 240 kg + 40 kg per crate |
| Hotbar 1-5 | item id disimpan di `characters.hotbar`, dipakai lewat `/hs 1..5`, tidak pernah menunjuk nomor slot |
| Transfer stash | Ambil/Simpan 1 / 5 / semua, header kolom + `>> Rapikan` |
| Anti-dupe container | kunci satu-editor (`Lock_*`) + "act then verify" `cache_affected_rows()` + rollback otomatis |
| Drop milik sendiri | eksklusif 60 detik, lalu publik agar item tidak jadi sampah |
| Loot zombie + peti dunia + ikon peta | pickup tipe 2 + `Loot_TakeNearest`; 3 peti langsung saat spawn, maks 8/lobby; 2 tingkat: peti biasa (biru) & **peti langka** (emas, 14% siang / 22% malam, isi `gRareLoot`) |
| Budget NPC dinamis (FCNPC 80/100 ms) | 1 pemain ±52 zombie, bukan 190; sync 80 ms solo, 100 ms saat ≥3 pemain; pathfinding raycast hanya jika garis terhalang |
| Crafting 17 resep + perbaikan alat/senjata | `/craft`, refund otomatis kalau gagal; item baru: Energy Drink, Radio, Decoy, Adrenaline, Vaksin, Toolbox |
| Kendaraan liar 9/lobby (35% rusak berat) | plat `WRECK` = mesin mati + BBM kosong + bodi rusak; perbaiki dengan Toolbox/Repair Kit + BBM jerrycan/pom |
| Radio lobby | `/radio <pesan>` siaran satu lobby, butuh item Radio (craft atau drop Screamer) |
| Revive antar pemain | `/revivekan` di dekat teman downed (3,5 m) + 1 Bandage, +15 XP penolong |
| Barter antar pemain | `/trade [id]` — panel dialog dua pihak, item + uang, konfirmasi ganda anti-scam, ledger penuh |
| Party co-op | `/party`, `/invite`, `/accept`, `/leave`, `/kick`, `/p <pesan>` — maks 4 orang, friendly fire mati, 20% XP dibagi ke anggota ≤60 m, panel HP/jarak/status tumbang |
| NPC rekrutan | `/rekrut` di perekrut kamp ($600 + 2 makanan + 1 air) → rekrutan FCNPC ikut, menembak zombie ≤26 m, bisa tumbang & ditolong `/teman revive`, bertahan lintas relog |
| Admin berlevel | `accounts.admin_level` (0 pemain / 1 admin / 2 superadmin), dimuat saat login · `/admin` info · `/setadmin <id> <0-2>` (superadmin) · pemilik `ferzdevz` otomatis superadmin saat daftar/login |
| Moderasi | `/ban <id> <menit\|0> [alasan]` (permanen/sementara, admin tak bisa ban admin), `/unban <nama>` (superadmin), `/bans` riwayat · auto-unban saat masa blokir habis |
| Papan peringkat | `/top` — 4 kategori sekaligus: kill zombie, uang, jam main, rank point (satu query UNION ALL) |
| Peti party | `/peti` buka, `/peti taruh` simpan · container bersama tersimpan di karakter pemimpin party (`party_stash`), anggota harus dalam 8 m |
| Tier rekrutan | `/teman upgrade` — tier 1/2 memakai kain, elektronik, repair kit, amunisi rifle + uang; HP 100→160→220, damage 13→19→26 |
| Bar HP boss | HUD atas-tengah menampilkan `ABOMINATION xx%  #####-----`; di bawah 50% boss memanggil 5 runner (fase 2, sekali per boss) |
| Superadmin `/sa` | Panel dialog penuh, 7 kategori: Pemain · Zombie · Item · Diri sendiri · Dunia & event · Moderasi · Server — semua aksi tanpa mengetik command |
| Panel › Pemain | kebal, pulihkan, +$1k/10k/100k, uang 0, +100/1000 XP, skill 5, freeze, teleport ke/tarik, beri item, tendang, ban 30m/permanen |
| Panel › Zombie | 8 tipe (walker…Abomination) × jumlah 1/3/5/10/25/50, bersihkan lobi, spawn horde 8 zombie |
| Panel › Item | 6 kategori, daftar berhalaman (id + nama + berat), jumlah 1/5/10/25/50 |
| Panel › Dunia | supply drop, blood moon, quiet hour, horde, jam 06/12/18/22/00 |
| Panel › Teleport | hub, gudang, craft, vendor, ekstraksi, shelter, base sendiri, isi bensin & perbaiki kendaraan terdekat (tanpa ketik koordinat) |
| Panel › Moderasi | 6 ban aktif terakhir (klik = buka ban) |
| Superadmin perkakas | `/god` `/spawnz <tipe> [n]` `/zclear` `/giveitem` `/give` `/itemlist` `/setcash` `/setxp` `/setskill` `/max` `/tppos` `/goto` `/gethere` `/freeze` `/sethour` — semua tercatat di log |
| Backup harian | cron `0 4 * * *` menjalankan `scripts/backup.sh` → `backups/cron.log` |
| Boss malam | **Abomination** (2400 HP, damage 45) muncul maks 1 per lobi saat malam, cooldown 15 menit, loot tabel khusus + jaminan *Specimen*; hadiah $250 + 60 XP + 3 scrap |
| Loot misi: dokumen & gudang | `Dokumen Rahasia` (jual $180 / kontrak), `Kunci Gudang` (spawn peti rare di luar kamp), `Buku Catatan` (+1 progres dokumen) |
| Kontrak harian 4 slot | 3 kontrak zombie + 1 kontrak dokumen; hadiah dokumen = uang besar + Kunci Gudang |
| Base raid terarah | Horde memilih **base ber-heat tertinggi** (noise di dalam petak base), zombie di-anchor ke petak, hadiah $420 + 35 XP kalau raid dipatahkan |
| Kill streak | 5 zombie/menit = +15 XP, 10 zombie/menit = +30 XP +2 scrap (toast RAMPAGE) |
| Barricade persisten (HP + dicakar zombie) | tabel `barricades`, dimuat ulang saat boot |
| Horde Director 2.0 (heat per zona + screamer) | `[HORDE]` toast, heat dari radius tembakan |
| Pedagang kamp (beli/jual) | `/shop`, cash + material |
| Anti-cheat posisi/teleport/rapid-fire | flag ke `anticheat_flags`, freeze lalu kick bertingkat |
| ColAndreas collision penuh | `[BOOT] ColAndreas aktif: raycast uji model 4830` |
| Skill tree 5 cabang (3 level) | `/skill`, poin dari level up, efek langsung ke damage/heal/noise/harga jual |
| Shelter pribadi (interior + virtual world) | `/shelter`, zombie tidak bisa masuk, stash/craft/pedagang tetap jalan |
| Event sosial tiap 5 menit | Supply Drop, Blood Moon, Jam Tenang; `/event` untuk admin |
| Deteksi DPS-spike | >320 damage/detik → flag `DPS_SPIKE`, damage dibatalkan |
| Base Building (7 struktur, HP, tier 1-3) | `/build`, tabel `base_structures`, crate menambah slot stash, zombie merusak lalu `/fix` |
| Raid horde terarah ke base | `/raidbase` (admin) + otomatis saat heat zona base ≥ 30, log `[HORDE] raid base char=…` |
| `/stats` dari `kill_log` | zona/senjata/tipe zombie teratas, jarak tembak ikut tersimpan |
| Luka terbuka & perdarahan | 0–3 stack, DoT tiap 5 dtk, bandage 1 / medkit semua, persist ke DB |
| Generator base | `/generator`, BBM liter, hanya malam; turret mati tanpa daya |
| Escort trader | `/escort` di luar kamp, antar NPC balik ke kamp, hadiah cash/scrap/XP |
| Misi onboarding | 5 langkah persisten (bunuh 3 zombie → loot → craft → 3 sampel → ekstraksi) via `/misi`, reward tiap langkah + $300 + 5 scrap final |
| Mapping modern | Teleport shelter anti-jatuh (`Streamer_UpdateEx` preload), cap 920 objek klien, kamp diretexture (dinding beton + papan neon SAFE ZONE + barikade), 2 Dynamic Actor pedagang, pintu fisik shelter auto-buka |
| Respec skill | `/respec`, semua level → poin, biaya Scrap 3 |
| Self-test 26 pemeriksaan (registry, tanah, tabel, kolom baru) | otomatis 8 dtk setelah boot + `/selftest`, log PASS/FAIL |
| Rancangan Base Building lengkap | `docs/BASE_BUILDING.md` (aturan penempatan, biaya, anti-abuse, fase lanjut) |

Yang **belum**: load test 100 pemain (fase 8), migrasi open.mp (runtime sekarang SA-MP 0.3.7 stabil). Interior shelter custom dan leaderboard musiman (kategori KILL MUSIM INI di `/top`) sudah diimplementasikan 26-11-19.

## Perintah dalam game

| Perintah | Fungsi |
|---|---|
| `/help` | daftar perintah |
| `/interact` atau tombol **Y** | hotspot terdekat (stash, craft, pedagang, ekstraksi) |
| `/take` | ambil loot terdekat (mobile) |
| `/ekstrak` | serahkan minimal 3 sampel zombie di titik ekstraksi |
| `/garasi` | beli/keluarkan kendaraan di garasi kamp |
| `/simpan` | simpan kendaraan yang dikemudikan/dekat ke garasi |
| `/isi [liter\|kan]` | isi tangki kendaraan atau jerrycan di pom ($5/L) |
| `/perbaiki` | perbaiki kendaraan dengan Repair Kit + Scrap 3 |
| `/sedot` | pindahkan bensin mobil terbengkalai ke jerrycan |
| `/inv` | tas + pakai/buang (dialog berheader) |
| `/tas` | kapasitas beban, tingkat tas, sisa kg |
| `/pakai <nama\|nomor>` | pakai item tanpa dialog (mobile) |
| `/buang <nama\|nomor> [n]` | buang 1 atau sebagian item |
| `/rapikan` | gabung tumpukan sejenis + rapatkan slot |
| `/hotbar` | panel 5 slot hotbar cepat |
| `/hotbar set <1-5> <item>` | isi slot hotbar (tersimpan di DB) |
| `/hs <1-5>` | pakai item hotbar tanpa buka tas |
| `/inv` → `Slot hotbar` | tandai item ke hotbar tanpa mengetik nama |
| `/peti` | peti party: Ambil/Simpan 1 / 5 / semua, maks 240 kg, satu editor |
| `/stash` | peti 60 slot + 20 per crate di base (maks 120) |
| `/craft` | meja crafting (hub atau Bench di base) |
| `/shop` | pedagang (hub atau shelter) |
| `/skill` | skill tree: Marksman, Toughness, Scavenger, Medic, Stealth |
| `/stats` | statistik karakter: zombie dibunuh, zona, senjata, tipe |
| `/respec` | kembalikan semua level skill jadi poin (Scrap 3) |
| `/shelter` (alias `/rumah`) | masuk/keluar shelter pribadi yang aman |
| `/base` | info base: tier, struktur, integritas, kapasitas stash |
| `/build` | bangun Wall/Gate/Crate/Bench/Bed/Turret/Generator (biaya material) |
| `/upgrade` | naik tier base (tier 2 buka Bench/Generator, tier 3 buka Turret) |
| `/generator` | isi BBM generator dari jerrycan (dekat generator) |
| `/escort` | mulai escort trader di luar kamp; antar kembali ke kamp |
| `/misi` | lihat misi onboarding aktif dan progresnya (5 langkah, persisten) |
| `/bongkar` | bongkar struktur terdekat, refund 50% material |
| `/fix` | perbaiki struktur base atau barricade terdekat (Scrap 2) |
| `/veh [model]` | spawn kendaraan uji |
| `/test`, `/zspawn`, `/horde` | admin: spawn zombie/horde |
| `/diag`, `/ac` | admin: telemetri, ms tick tiap sistem (akhir/puncak), flag anti-cheat |
| `/selftest` | admin: jalankan 25 pemeriksaan konfigurasi + tabel DB |
| `/raidbase` | admin: paksa raid 12 zombie ke base sendiri (uji wall/turret) |
| `/event` | admin: paksa event berikutnya sekarang |

## Sub-lobby

Satu IP, satu proses, dua dunia. Pindah lobby di kamp lewat `/lobby`; karakter, item,
dan base tetap sama, hanya dunia (zombie, loot, kendaraan, pemain lain) yang berbeda.
Detail di `docs/LOBBIES.md`.

## Survival: tubuh & kendaraan

HUD menampilkan lapar, haus, dan kandung kemih (`/kencing`); makan/minum mengisi meter
dan nilai disimpan di DB. Kendaraan liar spawn sendiri di zona (maks 5/lobby), 45%
terkunci, dan dibuka dengan Lockpick. Bahan bakar dihitung dalam liter memakai tangki
dan konsumsi per model; isi hanya di pom kamp ($5/L) atau dengan jerrycan. Garasi
menyimpan 4 kendaraan milik pemain lengkap dengan bensin dan kondisi, dengan asuransi
saat mobil hancur. Kendaraan rusak (<350 HP) bisa mati mendadak dan diperbaiki
dengan Repair Kit + Scrap 3; bensin mobil terbengkalai bisa disedot ke jerrycan.

**Luka terbuka** menumpuk sampai 3 dan berdarah tiap 5 detik; bandage menutup
satu luka, medkit menutup semuanya. Luka disimpan ke DB, jadi logout bukan cara
menghilangkannya. **Generator base** hanya membakar BBM saat malam; turret tanpa
daya berhenti menembak. **Escort trader** memanggil NPC di luar kamp untuk
diantar pulang; zombie yang mendekat melukai trader, dan hadiahnya dibayar hanya
jika dia selamat sampai kamp.

**Ekstraksi berbasis sampel**: zombie menjatuhkan *Zombie Specimen* (8% zombie
biasa, 35% Brute/Stalker). Bawa minimal 3 (maks 8) ke titik ekstraksi untuk
$120/sampel + $100 bonus. Sampel tidak bisa masuk stash; membawa sampel menambah
tekanan zombie Director, dan setiap ekstraksi menambah panas zona. Cooldown 90
detik per lobby. Detail di `docs/SURVIVAL.md`.

## Core loop: makam & kontrak

Mati menjatuhkan seluruh tas sebagai **makam** (20 item, 10 menit, pemilik eksklusif
2 menit, tersimpan di MySQL dan dimuat ulang saat boot). Tiga **kontrak harian** per
karakter memberi target kill dan hadiah uang/item/XP, deterministik per hari sehingga
restart tidak mengubahnya. Detail di `docs/CORE_LOOP.md`.

## Loot

Peti dunia punya ikon peta kuning dan tiga peti pertama dibuat saat pemain spawn,

Zombie memakai tabel bobot per tipe dengan loot berbeda untuk zona tier 2/3, plus bonus
langka yang naik di malam hari. Dunia memunculkan peti acak (objek kayu + ikon koper,
draw distance 60 m) maksimal 6 per lobby. Brute dan Stalker meninggalkan **mayat** yang
digeledah lewat dialog (`/geledah` / tombol Y), bukan tumpahan objek. Detail di
`docs/LOOT.md`.

## Data ColAndreas

`scriptfiles/colandreas/ColAndreas.cadb` wajib ada (huruf kecil, 54 MB, tidak masuk repo).
Lihat [docs/RUNTIME.md](docs/RUNTIME.md) §4 untuk aturan versi dan verifikasi boot.

## Mulai cepat

```bash
docker compose up -d            # MariaDB 10.11
./scripts/run-server.sh         # build + jalankan (butuh LD_LIBRARY_PATH untuk mysql.so)
```

## Backup & restore

```bash
./scripts/backup.sh                          # dump laststand + laststand_s2 → backups/<stamp>/
BACKUP_KEEP=14 ./scripts/backup.sh           # simpan 14 backup terakhir
./scripts/restore.sh                         # pakai backup terbaru (minta ketik RESTORE)
./scripts/restore.sh backups/20261118_225519 # folder tertentu
./scripts/restore.sh <dir> --yes             # tanpa konfirmasi (untuk cron/otomasi)
```

Isi tiap backup: `laststand.sql.gz`, `laststand_s2.sql.gz`, `server.cfg` + `mysql.ini`
(root/s1/s2), `SHA256SUMS` (diverifikasi otomatis sebelum restore), dan `MANIFEST`
(stamp, daftar DB, sha256 `main.amx`). Restore otomatis membuat snapshot
`backups/pre_restore_<stamp>/` sebelum menimpa data live.

Backup harian via cron (opsional, jalankan di host):

```bash
0 4 * * * cd /home/boxd/zombie-samp && ./scripts/backup.sh >> backups/cron.log 2>&1
```

## Open source

Repo ini **memang** menyimpan semuanya: source, plugin `.so`, `samp03svr`,
dependency, data ColAndreas 51 MB, tooling agen, contoh runtime, dan kredensial
development. Clone → jalan, tanpa unduhan tambahan. Kredensial di repo
dianggap publik — lihat [SECURITY.md](SECURITY.md) untuk daftar rotasi wajib
sebelum dipakai produksi.

* Lisensi: [MIT](LICENSE) — bebas dipakai, dimodifikasi, dan dijual.
* Cara berkontribusi + gate wajib (build, selftest, soak, memori):
  [CONTRIBUTING.md](CONTRIBUTING.md).
* Pertanyaan arsitektur: buka issue dengan label `design`.
