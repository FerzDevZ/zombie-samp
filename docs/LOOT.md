# Sistem loot

Dua sumber loot, dua tabel berbeda, semuanya di-roll di server.

## 1. Loot zombie (`config/loot_tables.inc`)

`gZombieLoot[tipe][slot]` berisi `{item, bobot, min, max}`. Bobot relatif, bukan persen.
Jumlah roll per tipe:

| Tipe | Roll | Catatan |
|---|---|---|
| Walker | 0–2 | 20% tidak membawa apa pun |
| Runner | 1 | — |
| Crawler | 0–1 | 15% kosong |
| Brute | 2–3 | bahan bangunan, kit, medkit |
| Bloater | 1–2 | kimia, herbal, obat |
| Screamer | 1–2 | elektronik, amunisi, blueprint |
| Stalker | 1–2 | darah, antibiotik, amunisi rifle |

Bonus langka di-roll terpisah: dasar 8%, +1% per tier zona, +4% saat malam, maksimum 25%.
Isinya `gRareLoot`: blueprint, medkit, blood pack, amunisi rifle, barricade kit.
Setiap drop diberi jitter ±0,7 m supaya tidak menumpuk di satu titik.

### Loot menurut tier zona

Tier 2 dan 3 mengganti dua slot terakhir tabel dasar:

| Tier | Slot bonus 1 | Slot bonus 2 |
|---|---|---|
| 1 | tabel dasar | tabel dasar |
| 2 | Medkit (bobot 12) | Rifle ammo x3–8 (bobot 10) |
| 3 | Blood pack (bobot 12) | Blueprint suppressor (bobot 6) |

Bobot rifle ammo di semua tabel sudah diturunkan ~25% supaya amunisi rifle tetap
langka dibanding pistol ammo.

### Mayat elite (Brute & Stalker)

Brute dan Stalker **tidak** menumpahkan item ke tanah. Saat mati mereka membuat mayat:

- Maksimum 24 mayat, umur 5 menit, per lobby.
- Menyimpan 2–3 jenis item (Brute 2–3 roll, Stalker 2 roll), item sama digabung.
- Visual: ikon koper + label `MAYAT BRUTE` / `MAYAT STALKER`, terlihat sampai 40 m.
- Dibuka dengan tombol **Y** dalam 3 m atau `/geledah` (wajib untuk mobile).
- Dialog `2951`: daftar item, tombol Ambil; jarak diperiksa ulang saat mengambil.
- Mayat kosong langsung dihapus; mayat tertua dibuang kalau meja penuh.

## 2. Peti dunia (`loot/nodes.inc`)

- Maksimum 8 peti per lobby, interval 20 detik, umur 3 menit (ikut `Loot_ExpireTick`).
- Saat pemain pertama spawn, lobby langsung diisi **3 peti** supaya dunia tidak kosong.
- Titik dipilih acak di zona non-kamp (radius 40 m dari pusat zona), lalu dijatuhkan
  ke tanah lewat ColAndreas.
- Tidak pernah muncul dalam radius 12 m dari pemain: loot harus dicari, bukan gratis.
- Visual: objek peti kayu (model 1225) + ikon koper (1210) + label `PETI DARURAT`
  dengan draw distance 60 m. **Ikon peta kuning** (model 1239) menandai peti dari
  jauh, jadi pemain di HP tidak perlu menebak lokasinya.
- Tabel `gNodeLoot`: makanan, air, scrap, kain, bandage, amunisi, elektronik, medkit,
  amunisi rifle, blueprint.

## Aturan teknis

- `Loot_RollTable` mengembalikan `false` kalau total bobot 0; tidak ada drop diam-diam.
- Meja loot penuh (`MAX_LOOT_DROPS 300`) → drop tertua dibuang lewat `Loot_PurgeOldest`,
  bukan drop baru yang dibuang.
- Pickup tipe 2 (walk-over) + `/take`; filter world/lobby dan `playerid` pemilik.
- Setiap drop punya world lobby-nya sendiri (`Lobby_WorldById`).

## Uji

- `/lootnode` (admin) menyebar 3 peti di lobby sendiri.
- `/selftest` memeriksa semua tabel loot punya bobot > 0 dan item id valid.
