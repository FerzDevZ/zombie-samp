# Multi-server: banyak dunia, satu VPS, satu basis kode

LAST STAND bisa dijalankan sebagai beberapa server sekaligus (mis. server 1 dan server 2)
di mesin yang sama. Yang dipisah: port, database, nama server, log, proses.
Yang dipakai bersama: **satu** `main.amx`, plugin, dan data collision `ColAndreas.cadb`.

## Peta instans saat ini

| Instans | Port UDP | Database | Nama server |
|---|---|---|---|
| `s1` | 7777 | `laststand` | LAST STAND 1 - Quarantine |
| `s2` | 7778 | `laststand_s2` | LAST STAND 2 - Outbreak |

Akses dari Tailscale (jalur utama untuk HP): **`100.85.253.77:7777`** dan
**`100.85.253.77:7778`**. IP lama `100.122.225.77` milik host `samp-server`
yang sekarang offline — jangan dipakai lagi.

Tailscale di VPS ini dipasang `tailscale up --accept-dns=false
--hostname=laststand-vps`. Query protokol SA-MP ke IP tailnet sudah diuji
menjawab di kedua port, jadi tidak perlu membuka UDP di AWS Security Group.
Tidak ada port TCP yang dibuka — SA-MP hanya UDP.

## Struktur direktori

```
runtime/s1/
  server.cfg          konfigurasi instans (port, hostname, rcon)
  mysql.ini           database instans
  samp03svr -> ../../samp03svr        symlink ke binary
  plugins -> ../../plugins            symlink
  gamemodes -> ../../gamemodes        symlink: satu main.amx untuk semua
  scriptfiles/colandreas -> ../../scriptfiles/colandreas   cadb 52 MB bersama
  server_log.txt      log instans (bergilir otomatis oleh server)
  console.log         stdout proses
  logs/errors.log     log aplikasi
runtime/s2/ ...
```

State pemain, base, dan ledger **tidak** di runtime: semuanya di database masing-masing.
Karena itu `runtime/` boleh dihapus dan dibuat ulang tanpa kehilangan progres.

## Akun lintas server (`server_id`) — migrasi `026_server_instance.sql`

Dua model yang mungkin:

| Model | Database | Karakter | Kelebihan | Kekurangan |
|---|---|---|---|---|
| DB terpisah (dipakai sekarang) | `laststand`, `laststand_s2` | per DB | isolasi penuh, backup terpisah | pemain harus daftar ulang, progres tidak pindah |
| **DB bersama + `server_id`** | satu `laststand` | satu tabel, kolom `server_id` | satu akun/login/ban berlaku semua server | butuh filter di setiap query karakter |

Sekarang keduanya didukung: `characters.server_id` (default 1) plus `instance.inc`.

```ini
# runtime/s1/scriptfiles/instance.ini   (bukan server.cfg!)
server_id = 1
# runtime/s2/scriptfiles/instance.ini
server_id = 2
```

- `Server_InstanceId()` membaca `instance.ini`, lalu turunan port listen
  (`port 7777` → instance 1, `7778` → 2), lalu 1. Dijepit ke 1..9.
- **Kenapa bukan server.cfg**: SA-MP membuang direktif yang tidak dikenalnya,
  jadi `server_id 2` di `server.cfg` tidak bisa dibaca gamemode —
  terbukti dari s2 yang tetap melaporkan `server_id=1` sampai nilainya
  dipindah ke `instance.ini`. `scripts/instance.sh` menulis file ini otomatis
  untuk instance baru.
- Path file di Pawn relatif terhadap folder `scriptfiles/` (fungsi file
  dikurung di sana): tulis `instance.ini`, **bukan** `scriptfiles/instance.ini`.
- Filter diterapkan di tiga tempat: daftar karakter, muat karakter (`/pilih`),
  dan INSERT pembuatan karakter. `/pilih <id>` milik instance lain = 0 baris.
- `accounts` sengaja **tidak** diberi `server_id`: login, password, dan ban
  berlaku lintas instance (itu inti "akun lintas server").
- Bukti isolasi (fixture 1 akun, 2 karakter):

```
--- instance 1        id 6  ZZ_S1_Char  server_id 1
--- instance 2        id 7  ZZ_S2_Char  server_id 2
--- pilih lintas instance              rows = 0
fixture bersih: chars=0 accounts=0
```

## Antrean saat penuh (`core/slotqueue.inc`)

SA-MP memakai slot saat koneksi masuk, jadi tidak ada cara menolak koneksi
sebelum slot terpakai. Antrean karena itu bekerja di tingkat gamemode:

```cfg
player_cap 90      # batas pemain bermain; maxplayers 300 - maxnpc 200 = 100 slot
```

- `OnPlayerConnect` menandai pemain sebagai menunggu bila `Server_PlayingCount() >= player_cap`.
- Saat menunggu, `/masuk` dan `/daftar` ditolak dengan pesan posisi antrean
  (`Queue_BlockAuth`) — tidak ada jalur auth yang lolos.
- `Queue_Tick` (5 detik) mengizinkan satu pemain terdepan (FIFO) begitu ada slot,
  lalu mengirim pesan + toast "slot terbuka".
- Menunggu > 10 menit = keluar dengan pesan jelas (`KickQueuedPlayer`), bukan
  ditendang diam-diam.
- Selftest batas: `cap=90, tahan di 89/90/91 = 0/1/1`.

Yang **tidak** bisa dikerjakan dari dalam gamemode: load balancer DNS/health-check
yang mengarahkan pemain ke server kosong. Itu pekerjaan infrastruktur (proxy UDP /
launcher), bukan PAWN; yang tersedia di sini adalah antrean per instance.

## Dekorasi kamp (`/decor`, ROADMAP 223)

Dekorasi memakai model yang sudah terbukti tampil di server ini (19447 dinding,
1427 barikade, 19477 papan tanda). Verifikasi visual dilakukan di client:

```
/decor              # daftar 9 dekorasi + koordinat
/decor tp 3         # pindahkan diri 6 m dari dekorasi #3
```

Kalau ada prop yang tidak muncul di client, lapor nomor indeksnya — tabel
`DecorProp`/`DecorSign` di `world/decor.inc` adalah satu-satunya tempat yang perlu diubah.

## Masuk dari HP (jaringan berbeda)

VPS ini di balik NAT: `ens5` memegang IP privat `172.31.4.237`, IP publiknya
`3.147.85.241`. Dari HP **jangan** pakai IP privat.

| Instance | Alamat di client HP |
|---|---|
| s1 | `3.147.85.241:7777` |
| s2 | `3.147.85.241:7778` |

1. Client SA-MP Mobile → **Add server** → isi alamat di atas → Connect.
2. Server sudah listen di `0.0.0.0:7777` dan `0.0.0.0:7778`, bukan `127.0.0.1`.
3. Di host, `ufw` nonaktif dan `iptables INPUT` = `ACCEPT` — host tidak memblokir.
   Yang wajib dibuka: **AWS Security Group** instance EC2, inbound
   `UDP 7777-7778` dari `0.0.0.0/0` (atau IP HP `/32`). Tanpa itu client
   berhenti di "Retrieving info..." walau server sehat.
4. `announce 0` = server tidak muncul di daftar Internet SA-MP; koneksi langsung
   lewat IP tetap bekerja. `announce 1` hanya untuk masuk masterlist.

Alternatif tanpa membuka port game: host sudah punya tunnel WireGuard
(`wg0`, `10.77.156.1`, listen UDP 51820). Daftarkan HP sebagai peer, lalu pakai
`10.77.156.1:7777` / `:7778`.

Bukti server menjawab (query protokol SA-MP, paket `SAMP`+opcode `i`):

```
7777: RESPON
7778: RESPON
```

Kalau HP masih gagal padahal query lokal menjawab, masalahnya jalur jaringan
(Security Group / NAT), bukan gamemode.

## Perintah

```bash
./scripts/instance.sh list                 # tabel port, DB, status, RAM
./scripts/instance.sh start s1             # atau: start all
./scripts/instance.sh stop s2              # atau: stop all
./scripts/instance.sh restart all
./scripts/instance.sh logs s1              # 25 baris terakhir server_log.txt
./scripts/instance.sh init s3 7779 laststand_s3 "LAST STAND 3 - Nightfall"
./scripts/instance.sh db s3                # terapkan migrasi ke DB baru (hanya jika kosong)
```

`init` membuat direktori, symlink, `server.cfg`, `mysql.ini`, lalu database + semua migrasi.
Database dibuat oleh user `root` MariaDB (user aplikasi sengaja tidak punya izin
`CREATE DATABASE`); tabel dibuat oleh user aplikasi.

## Menambah server ke-3

```bash
./scripts/instance.sh init s3 7779 laststand_s3 "LAST STAND 3 - Nightfall"
./scripts/instance.sh start s3
./scripts/instance.sh list
```

Selesai. Tidak ada perubahan kode: `main.amx` yang sama dipakai semua instans.

## Deploy kode baru ke semua instans

```bash
sampctl build
./scripts/instance.sh restart all
```

`main.amx` dibaca saat boot, jadi semua instans harus direstart agar memakai versi baru.

## Yang harus dijaga

| Aturan | Alasan |
|---|---|
| Satu database per instans | Dua server yang menulis baris `characters` yang sama akan saling menimpa posisi/HP terakhir. |
| `hostname` tanpa `#` | `#` adalah awal komentar di `server.cfg`, nama server terpotong. |
| Port unik per instans | Bentrok port membuat instans kedua gagal bind dan keluar. |
| RAM ±520 MB per instans | 190 NPC + cadb. Dua instans ±1 GB; VPS 2 GB masih cukup, 1 GB tidak. |
| Restart lewat `instance.sh` | PID dikelola per instans; `pkill samp03svr` mematikan semua sekaligus. |

## Kapasitas

Terukur di VPS ini (8 vCPU, 32 GB): 2 instans × 190 NPC = 380 NPC, RSS 517 MB per instans,
boot + self-test 19/19 PASS di kedua instans, tanpa runtime error.
Batas praktis lebih ditentukan CPU (AI zombie) daripada RAM: satu instans dengan 190 NPC
memakai sekitar 1 core; di 8 core, 4–5 instans masih aman.

## Akun lintas server: keputusan yang sudah diambil

Dua model dibahas sebelum `server_id` ada. Yang dipakai sekarang: **DB bersama +
`characters.server_id`** untuk karakter, sementara `accounts` tetap bersama dalam
satu database (lihat bagian di atas). Model "sinkronisasi offline antar DB"
ditolak karena pemain tidak bisa langsung main di server baru dengan akun lama.

Peringatan yang tetap berlaku: **jangan** mengarahkan dua instance ke database
yang sama tanpa filter `server_id` di setiap query karakter — itu bukan
multi-server, itu dua server yang saling merusak state.

## Kapan sebaiknya 2 server, kapan 1 server?

Beban bukan alasan untuk memilih 1 server di mesin ini. Pengukuran saat 190 NPC hidup
dan 0 pemain, dua instans berjalan bersamaan:

| Metrik | Nilai terukur |
|---|---|
| CPU per instans | 5,3–6,3% dari 1 core |
| RAM per instans | 517–525 MB |
| Load average 8 vCPU | 0,15 |
| Thread per instans | 9 |

Artinya dua server memakai sekitar 11% CPU dan 1 GB RAM: jauh dari overload.
Yang mahal bukan jumlah instans, tapi **jumlah pemain**: tiap pemain menambah zombie
yang mengejar, tembakan, dan sync. Perkiraan kasar: +20 pemain ≈ +10–15% CPU per instans.

Alasan memilih **1 server publik** justru bukan performa, tapi desain permainan:
mode survival bergantung pada kepadatan pemain (horde, event, suara tembakan, base).
20 pemain di satu dunia jauh lebih hidup daripada 10 + 10 di dua dunia.

| Situasi | Pilihan |
|---|---|
| < 120 pemain bersamaan | 1 server publik (s1). Server 2 dimatikan atau dipakai uji internal. |
| 120–200 pemain, satu dunia terasa penuh | buka s2 sebagai overflow, akun terpisah |
| Ingin dua aturan main berbeda (normal vs hardcore/PvP) | 2 server sejak awal, promosikan sebagai mode berbeda |
| Butuh menguji build tanpa mengganggu pemain | s2 = staging (0–5 pemain internal) |

Ambang waspada, dicek dengan `./scripts/instance.sh list` dan `ps -o pcpu=`:

- CPU satu instans > 70% selama > 5 menit dengan pemain < 200 → turunkan `maxnpc`
  (mis. 190 → 120) atau kurangi `MAX_ZOMBIES` di `config/constants.inc`.
- RAM satu instans > 1,2 GB → curigai kebocoran objek/NPC (base structure, korps).
- `logs/errors.log` bertambah atau ada `fcnpc-crash_*` baru → masalah NPC, bukan kapasitas.

Mematikan server kedua saat tidak dipakai: `./scripts/instance.sh stop s2`
(menghemat ±520 MB dan satu core; data di database tetap utuh).
