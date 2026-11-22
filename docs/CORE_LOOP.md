# Core loop: kematian, makam, kontrak harian

Dua sistem yang membuat loop "keluar kamp → cari loot → kembali" punya risiko dan tujuan.

## 1. Makam pemain (`loot/grave.inc`)

Mati tidak lagi menghapus tas. `OnPlayerDeath` dan timeout downed memanggil
`Grave_CreateFromPlayer` **sebelum** `Inventory_Clear`.

| Aturan | Nilai |
|---|---|
| Slot makam | maks 20 (seluruh isi tas) |
| Makam aktif | maks 16 per server |
| Umur | 10 menit |
| Eksklusif pemilik | 2 menit pertama |
| Ambil | tombol Y atau `/geledah`, radius 3 m |
| World/lobby | disimpan; hanya pemain di world + lobby sama yang bisa melihat/mengambil |

Setelah 2 menit, pemain lain di lobby dan world yang sama boleh mengambil — ini
konsekuensi extraction survival, bukan bug.

Visual: ikon koper + label `MAKAM <nama>` (40 m). Baris disimpan di `graves` +
`grave_items` (FK cascade), dimuat saat boot lewat `Grave_BootLoad`, jadi restart
server tidak menghapus tas pemain. Setiap perubahan isi ditulis ulang utuh
(`Grave_PersistItems`) karena slot bergeser saat item diambil.

Durabilitas item yang diambil ulang di-reset ke 100 — biaya kematian yang disengaja.

## 2. Kontrak harian (`progression/contracts.inc`)

Empat kontrak per karakter per hari, deterministik dari `(char_id, day_id)`
(slot 4 = dokumen rahasia: klaim WAJIB menyerahkan dokumen fisik sejumlah target):

| Slot | Jenis | Target | Hadiah |
|---|---|---|---|
| 0 | Zombie apa pun | 15–25 | Pistol ammo 12 + cash |
| 1 | Runner atau Brute | 5–11 / 1–3 | Bandage 2 / Scrap 3 + cash |
| 2 | Brute atau Screamer | 1–3 / 2–5 | Scrap 3 / Medkit 1 + cash |

- Dibuka dengan `/kontrak` atau menu pusat baris terakhir.
- Progres dihitung dari kill di zona non-kamp (sama dengan aturan XP).
- Klaim: cash + item + 15 XP; baris langsung ditulis ke DB.
- `INSERT IGNORE` + unique `(char_id, day_id, slot)` membuat restart tidak
  mengganti kontrak atau menghapus progres.
- Progres disimpan saat autosave (120 s), saat klaim, dan saat shutdown.

## Skema (migration 008)

- `graves(id, char_id, owner_name, lobby_id, world_id, pos_x/y/z, expires_at, created_at)`
- `grave_items(grave_id, slot, item_id, amount, durability)` FK cascade
- `contracts(char_id, day_id, slot, kind, target, progress, reward_*, claimed)` unique per hari

## Uji

1. `/zspawn 0 5` → mati (biarkan downed habis) → cek `[GRAVE]` di log, kembali ke lokasi, `/geledah`.
2. Relog setelah mati → makam tetap ada (dimuat dari DB).
3. Bunuh zombie di zona → `/kontrak` progres naik; selesaikan → Klaim → cash/item masuk.
4. Restart server di tengah kontrak → progres tidak berubah.
