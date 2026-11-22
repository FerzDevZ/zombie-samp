# Multi-server: banyak dunia, satu VPS, satu basis kode

LAST STAND bisa dijalankan sebagai beberapa server sekaligus (mis. server 1 dan server 2)
di mesin yang sama. Yang dipisah: port, database, nama server, log, proses.
Yang dipakai bersama: **satu** `main.amx`, plugin, dan data collision `ColAndreas.cadb`.

## Peta instans saat ini

| Instans | Port UDP | Database | Nama server |
|---|---|---|---|
| `s1` | 7777 | `laststand` | LAST STAND 1 - Quarantine |
| `s2` | 7778 | `laststand_s2` | LAST STAND 2 - Outbreak |

Akses dari Tailscale: `100.122.225.77:7777` dan `100.122.225.77:7778`.
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

## Akun lintas server (belum)

Saat ini akun dan karakter terpisah per database: pemain harus daftar di masing-masing server.
Kalau nanti ingin satu akun untuk semua server, pilihannya:

1. **Tabel akun terpusat** (`laststand_auth`) + kolom `server_id` pada `characters`, dan
   login membaca kredensial dari DB terpusat. Perlu migrasi + perubahan `account_auth.inc`.
2. **Sinkronisasi offline**: ekspor/impor `accounts` antar DB secara berkala. Murah, tapi
   pemain tidak bisa langsung main di server baru dengan akun lama.

Jangan arahkan dua instans ke DB yang sama sebelum `server_id` ada — itu bukan multi-server,
itu dua server yang saling merusak state.

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
