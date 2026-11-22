# RANCANGAN 2026 — LAST STAND (realistis, terukur, bisa dikerjakan)

Dokumen ini bukan daftar keinginan. Isinya: **angka nyata** dari mesin ini, **batas nyata**
platform SA-MP 0.3.7, **fitur yang benar-benar menambah retensi** dengan usaha yang wajar,
dan **optimasi yang dipilih dari bukti**, bukan dari feeling. Setiap butir punya gate yang
bisa diuji dan estimasi jam kerja satu orang + agent.

---

## 1. Kondisi terukur saat dokumen ini ditulis (19 Nov, 14:3x)

| Metrik | Nilai nyata | Sumber |
|---|---|---|
| Build | 0 error, 1 diagnostik pihak ketiga (colandreas) | `sampctl build` |
| Boot → siap | ~35–40 dtk, MySQL 136 ms | `server_log.txt` |
| Selftest | 36/36 PASS | log boot |
| RAM idle | 498–528 MB | `ps rss` |
| RAM @100 bot (puncak terukur) | **1350 MB** → ~8,5 MB/pemain | soak CSV |
| Tick zombie / player / hud | 0–1 / 1 / 1–3 ms | `[PERF]` |
| Tick director (puncak) | 24 ms | `[PERF]` |
| Zombie aktif saat 100 pemain | 90–94 | `[DIRECTOR]`, soak |
| **Crash @100 pemain** | SIGSEGV di `_IO_vsprintf`, **3× kejadian**, selalu saat RSS 1,26–1,50 GB | `fcnpc-crash_..._13.57.47 / 14.27.52 / 14.35.54 / 14.43.36.txt` |
| **Uji tanpa zombie @100 bot** | RSS 498 → **1260 MB** (~6,4 MB/pemain), lalu crash juga | soak `20261119-143901/samples.csv` |
| Ukuran AMX | 9,0 MB | `main.amx` |

### Akar crash yang baru ditemukan (bukan FCNPC, bukan Streamer)

Backtrace: `streamer.so(sampgdk_callback_invoke)` → samp03svr (callback → native `print`)
→ `_IO_vsprintf` di libc. Jadi plugin hanya pemicu; yang meledak adalah **`print(line)` di
`logger.inc`** yang meneruskan buffer runtime sebagai *format string*. Satu `%` di dalam data
(nick, label item, pesan bebas) membuat `vsprintf` membaca argumen yang tidak ada → SIGSEGV.

Perbaikan yang sudah dipasang (`logger.inc:32`): `printf("%s", line)`, build ulang, boot,
selftest 36/36. **Retest membuktikan perbaikan ini BELUM cukup**: crash yang sama tetap terjadi
(14:35 dan 14:43) — jadi format string adalah bug nyata yang harus diperbaiki, tapi **bukan
pemicu utama**.

Bukti yang mengarahkan ulang diagnosis:

- Tiga crash terjadi pada fase berbeda (spawn, stabil, disconnect massal) dengan backtrace identik.
- Semua terjadi saat RSS 1,26–1,50 GB; proses 32-bit tidak punya ruang alamat untuk tumbuh lagi.
- **Eksperimen kunci**: zombie dimatikan total (`ZOMBIE_SPAWN_DISABLED 1`), 100 bot tetap
  menaikkan RSS 498 → 1260 MB (±6,4 MB per pemain) dan tetap crash. Jadi biaya besar ada di
  **jalur per pemain**, bukan FCNPC/zombie.

Kesimpulan kerja: pemicu crash = **kehabisan memori 32-bit** yang membuat `malloc` gagal dan
`vsprintf` menulis ke pointer tak valid. Perbaikan format string tetap dipertahankan sebagai
hardening, tetapi P0 sekarang = menurunkan biaya memori per pemain dan memasang plafon aman.

---

## 2. Batas keras platform (yang menentukan "realistis")

| Batas | Angka | Konsekuensi desain |
|---|---|---|
| SA-MP 0.3.7 slot | satu pool untuk pemain + NPC; `maxplayers` 300 di sini | NPC maksimum = 300 − pemain aktif − reserve 8 |
| Proses 32-bit | ruang alamat < 4 GB, RSS aman < 1,5 GB | tidak boleh ada pertumbuhan per pemain > ~10 MB |
| Streamer | objek/pickup/label per pemain dibatasi `MAX_...` plugin; 1000 objek global masih wajar | tidak ada kota berlapis MLO besar; pakai interior terpisah |
| SA-MP Mobile | tanpa tombol F/klik bebas; dialog & chat jadi UI utama | setiap aksi wajib punya command fallback (sudah jadi invariant) |
| MySQL R41-4 | query non-threaded dilarang; pool 4 koneksi | autosave bertahap, bukan massal |
| Tanpa client di mesin build | visual/UX tidak bisa diverifikasi otomatis | fase playtest manual wajib sebelum publik |

Yang **tidak** dibangun karena tidak realistis di stack ini: CEF/HTML UI, custom model
0.3.DL, voice chat (sampvoice tidak jalan di mobile), matchmaking antar-server, anti-cheat
client-side, battle royale 100 pemain dengan drop-in.

---

## 3. Fitur realistis — dipilih karena menambah retensi, bukan karena keren

Prinsip: **perkuat loop yang sudah ada** (loot → risiko → ekstraksi → progresi) daripada
menambah subsistem baru. Urutan = prioritas kerja.

| # | Fitur | Kenapa realistis | Effort | Gate sukses (terukur) |
|---|---|---|---|---|
| F1 | **Hot zone harian** (2 zona loot ×1,5, ditandai di HUD) | pakai sistem loot+timer+deterministik kontrak yang sudah ada | 6–8 j | zona berpindah tiap hari, loot rate zona naik terbukti di log, 0 query baru per tick |
| F2 | **Ekstraksi bertingkat** (3 titik: cepat/berisiko, lambat/aman, malam-saja) | memperpanjang keputusan risiko tanpa sistem baru | 6 j | 3 titik aktif, bobot loot beda, gate malam hanya di titik C |
| F3 | **Kondisi senjata** (keausan per 100 tembakan, jamming 3%) | memakai durability item yang sudah ada | 5 j | keausan tersimpan di DB, jamming terpicu di log, tidak ada state baru di luar item |
| F4 | **Misi harian 3 slot** (bunuh/ekstrak/craft) | kontrak harian sudah ada; tambah jenis + reward kecil | 8 j | 3 misi/hari/karakter, selesai → reward ledger, tidak bisa diulang |
| F5 | **Event cuaca** (badai: zombie +30%, loot +20%, 5 mnt, tiap 3 jam) | satu tick global, tidak perlu entity baru | 4 j | event muncul di jadwal, multiplier terbaca di log, HUD menampilkan sisa waktu |
| F6 | **Safehouse upgrade** (2 tingkat: penyimpanan + tempat tidur) | base building sudah ada; ini hanya biaya + kapasitas | 6 j | storage 60→100 slot, tidur memulihkan stamina, biaya terpotong dari ledger |
| F7 | **Papan skor mingguan** (reset Senin) | `/top` sudah ada; butuh agregasi + tabel | 5 j | query agregasi <5 ms, reset terjadwal, data minggu lalu diarsipkan |
| F8 | **NPC pedagang keliling** (muncul 15 mnt tiap 2 jam, stok terbatas) | escort/trader sudah ada; ini penjadwalan | 4 j | muncul di 3 titik acak, stok habis, hilang saat waktu habis |
| F9 | **Pemulihan cedera** (patah tulang → lambat 20 mnt, bidai = craft) | wound system sudah ada | 4 j | status tersimpan, bidai memulihkan, tidak menambah entitas |
| F10 | **Kill feed + assist credit** | HUD sudah ada; tambah pesan | 3 j | assist dihitung dari damage ledger, tidak ada `OnPlayerUpdate` |

Total ±51 jam kerja fokus; realistis 2 minggu paruh waktu. **Semua** memakai tabel/entitas
yang sudah ada — tidak ada migrasi besar, tidak ada plugin baru.

Ditunda (butuh bukti pemain dulu): trading antar pemain berbasis UI penuh, sistem fraksi
polisi/medis, rumah sewa, kendaraan kustom, mini-game arcade.

---

## 4. Optimasi realistis — urut berdasarkan ROI dari angka di §1

Aturan: satu hipotesis → satu percobaan → satu ambang. Tidak ada optimasi tanpa before/after.

| # | Optimasi | Hipotesis (dari data) | Percobaan | Ambang terima | Effort |
|---|---|---|---|---|---|
| O0 | **Plafon RSS + restart terencana** (sudah dipasang) | crash selalu di RSS ≥1,2 GB; restart terencana menyelamatkan data | `supervise.sh` memantau RSS, restart di 1200 MB (env `MEM_LIMIT_MB`) | restart tercatat di `backups/supervisor.log`, 0 crash mendadak | **selesai** |
| O1 | **Format-string log** (sudah dipatch) | `print(buffer)` = bug nyata (SIGSEGV di `_IO_vsprintf`) | retest membuktikan crash tetap ada → bukan pemicu utama | tidak ada lagi `print(buffer)` di kode | **selesai, bukan pemicu utama** |
| **O2 (P0)** | **Audit biaya per pemain 6,4 MB** | dengan 0 zombie pun 100 bot = +760 MB, heap 812 MB | ukur 10/30/60 bot (slope), lalu matikan subsistem satu per satu (HUD, bag, stash, party) lewat saklar build | RSS @100 ≤700 MB | 10–14 j |
| O3 | **IO log ganda** | `print` + `fwrite` per baris = 2 syscall/baris; ribuan baris/menit | matikan `print` untuk LOG_INFO di build produksi | syscall log turun ≥50%, RSS stabil | 3 j |
| O4 | **Director tick 24 ms** | refresh grid tiap tick; 100 pemain = O(pemain×zombie) | cache posisi pemain 1 dtk, grid sel 30 m | p95 director <10 ms | 6 j |
| O5 | **Query pool** | pool 4; autosave 1 char/2 dtk | ukur queue `SHOW STATUS LIKE 'Threads_running'` saat soak | utilisasi <20%, tidak ada query >50 ms | 3 j |
| O6 | **Objek streamer** | dekat-pemain spawn menambah objek | hitung `CountDynamicObjects` per pemain | <400 objek/pemain, 0 "limit reached" | 4 j |
| O7 | **Autosave bertahap** | simpan semua saat disconnect massal | batch 20 karakter/2 dtk + flush saat shutdown | 0 kehilangan data, tick save <2 ms | 3 j |
| O8 | **Log rotation** | `server_log.txt` tumbuh tanpa batas | logrotate 14 hari | disk tidak naik >200 MB/minggu | 1 j |

O2–O4 dikerjakan hanya bila soak 20 mnt lulus tapi RSS/tick melanggar ambang; kalau lulus
semua, langsung ke fitur. Ini menjaga agar optimasi tidak menjadi proyek hobi.

---

## 5. Langkah realistis menuju server online (14 hari)

| Hari | Kerja | Gate |
|---|---|---|
| 1 | O0 plafon RSS (selesai) + **O2 diagnosis bertingkat**: 10/30/60 bot tanpa zombie, catat slope RSS | slope per pemain <1 MB |
| 2 | O2 lanjutan: matikan subsistem per pemain satu per satu (HUD → bag → stash/party → streamer) sampai slope turun; perbaiki sumbernya | RSS @100 ≤700 MB, soak 20 mnt tanpa crash |
| 2b | O4 director p95 (bila angka tick masih >10 ms) | p95 <10 ms |
| 3 | O3/O7 (log + autosave), O8 logrotate | syscall log turun, 0 kehilangan data saat restart |
| 4 | F1 hot zone + F5 cuaca (keduanya murni logika tick) | zona & cuaca terlihat di log + HUD, 0 query per tick |
| 5 | F3 keausan senjata + F9 cedera | status tersimpan, selftest tetap 36/36 |
| 6 | F4 misi harian + F7 papan mingguan | 3 misi/hari, agregasi <5 ms |
| 7 | F2 ekstraksi bertingkat + F8 pedagang | 3 titik + NPC terjadwal |
| 8 | F6 safehouse upgrade | kapasitas naik, ledger benar |
| 9 | Playtest internal: alur baru dari daftar→ekstraksi | tidak ada blocker gameplay |
| 10 | Playtest mobile (butuh perangkat user) | semua aksi punya fallback chat |
| 11 | Perbaikan hasil playtest + selftest + build gate | 0 error, selftest 36/36 |
| 12 | Go-live checklist E (supervisor, backup, rotasi kunci, firewall, Tailscale/playit) | kill -9 → hidup ≤40 dtk; restore DB teruji |
| 13 | Soak 60 mnt terakhir di konfigurasi final | semua gate Fase B lulus |
| 14 | Buka publik terbatas (10–20 pemain), pantau 24 jam | 0 crash, 0 assertion, backlog dicatat |

Hari 1–3 tidak menambah fitur: itu harga agar server tidak mati di menit kelima.

---

## 6. Risiko realistis & mitigasinya

| Risiko | Peluang | Dampak | Mitigasi (sudah/akan) |
|---|---|---|---|
| Crash lain muncul setelah O1 | sedang | server mati | supervisor 40 dtk + simpan `fcnpc-crash_*` tiap insiden + soak berkala |
| RSS naik >1,5 GB saat 100 pemain | sedang | OOM/32-bit mati | O2 audit, batasi spawn NPC, pool objek |
| Sandbox seccomp mematikan proses saat restart | tinggi di mesin ini | downtime | supervisor sesi (terbukti), jangan cron/systemd `--user` |
| playit.gg tidak stabil | tinggi | publik tidak bisa connect | Tailscale sebagai jalur utama; playit opsional |
| Tanpa client untuk QA visual | pasti | bug UX lolos | playtest manual hari 9–10 |
| 190 zombie tidak tercapai saat 100 pemain | pasti (matematika slot) | kepadatan turun | budget otomatis; naikkan `maxplayers` hanya setelah O2 |

---

## 7. Definition of Done (yang akan saya klaim "selesai")

1. Soak 60 menit @100 pemain: **0 crash, 0 assertion, p95 semua tick <20 ms, RSS <800 MB**.
2. Retest O1 membuktikan SIGSEGV `_IO_vsprintf` tidak muncul lagi (log + mtime `fcnpc-crash_*`).
3. F1–F9 selesai dengan gate masing-masing dan selftest tetap 36/36.
4. Playtest mobile lulus (butuh perangkat user — tanpa itu tidak diklaim).
5. Go-live checklist E tercentang, termasuk uji restore backup.
6. Semua insiden dicatat di `MEMORY.md` sebagai invariant, bukan sebagai cerita.
