# Sub-lobby (satu IP, satu proses, banyak dunia)

Ini **bukan** dua server terpisah. Satu proses `samp03svr`, satu IP, satu port (7777).
Pemain yang join bisa masuk ke dunia berbeda tanpa restart dan tanpa server kedua.

## Model

| Hal | Dibagi atau dipisah |
|---|---|
| Akun, karakter, item, stash, skill, base | **Dibagi** (satu database) |
| Posisi pemain, zombie, loot, kendaraan, barricade, struktur base | **Dipisah per lobby** |
| Chat, toast, horde, raid | **Per lobby** (tidak bocor antar dunia) |
| Port / IP / proses | **Satu** |

Pemisahan memakai **virtual world** dan **dynamic world** Streamer:

| Lobby | World | Tema | Zombie |
|---|---|---|---|
| 0 Quarantine | 100 | standar | kecepatan 1.00x, damage 1.00x |
| 1 Outbreak | 200 | lebih padat | kecepatan 1.15x, damage 1.35x |
| 2 Nightfall | 300 | nonaktif (reserved) | kecepatan 1.10x, damage 1.20x |

Shelter base memakai world sendiri:
`SHELTER_VW_BASE(1000) + base_id` (id DB global, unik lintas lobby).

## Aturan main

- Pindah lobby **hanya di kamp**, tidak saat di shelter atau di luar zona aman.
- Pindah lobby tidak menghapus progres; karakter, tas, dan base tetap sama.
- Base dibangun di lobby tempat pemain berdiri, dan tetap di lobby itu.
- Setelah restart, pemain masuk kembali ke lobby terakhirnya (`characters.lobby_id`).

## Perintah

- `/lobby` atau `/dunia` — daftar lobby + jumlah pemain, pilih untuk masuk.
- `/menu` → baris terakhir **Ganti lobby**.

## Implementasi

- `gamemodes/modules/config/lobbies.inc` — definisi lobby, `PlayerLobby`, getter.
- `gamemodes/modules/world/lobbies.inc` — dialog, world mapping, teleport, simpan DB.
- `zombie/spawn_director.inc` — target ambient dibagi proporsional per lobby; total
  zombie tetap dibawah `MAX_ZOMBIES`.
- `zombie/horde.inc` — `ZoneHeat[lobby][zone]`; tembakan di Lobby 1 tidak memanggil
  horde ke Lobby 2. Raid base juga per lobby.
- `loot/drops.inc`, `world/vehicles.inc`, `world/bases.inc`, `world/base_build.inc` —
  objek dibuat di world lobby pemiliknya.
- `security` — `Hash_NearestPlayer(..., lobbyId)` supaya zombie tidak pernah mengejar
  pemain di dunia lain.

## Catatan operasional

- Satu proses tetap memikul semua lobby: batas praktisnya `MAX_ZOMBIES` global (190),
  bukan jumlah lobby. Dua lobby dengan masing-masing 5 pemain tetap 190 zombie total.
- `MAX_LOBBIES 3` hanya batas array. Menambah lobby ke-4 berarti menaikkan konstanta
  itu dan menambah baris di `config/lobbies.inc`.
- Kalau nanti butuh mesin terpisah, `scripts/instance.sh` tetap ada untuk instans
  proses sendiri (port + DB sendiri), terpisah dari konsep lobby ini.
