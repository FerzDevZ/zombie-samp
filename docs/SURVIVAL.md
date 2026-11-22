# Survival: tubuh, garasi, bahan bakar, ekstraksi

## 1. Kebutuhan tubuh

HUD kiri menampilkan `LAP`, `HAUS`, `PIPIS`.

| Meter | Arti | Laju | Akibat saat ekstrem |
|---|---|---|---|
| `LAP` | hunger | −0.10 / 5 s (≈83 menit) | 0 → 0.8 HP / 5 s |
| `HAUS` | thirst | −0.14 / 5 s (≈60 menit) | 0 → 1.2 HP / 5 s |
| `PIPIS` | bladder | +0.12 / 5 s (≈69 menit) | 100 → 0.6 HP / 5 s |

- `Canned Food`: +30 lapar, +3 haus. `Water Bottle`: +42 haus, +12 pipis.
- `/kencing` (alias `/pipis`) mengosongkan kandung kemih; butuh ≥12%.
- Peringatan muncul di ambang kritis, maksimal sekali per 45 detik.
- Nilai disimpan di `characters.hunger/thirst/bladder`. Relog melanjutkan nilai
  terakhir; `0` adalah nilai sah (bukan diganti 100).
- Grace 15 detik pemain baru mematikan decay dan damage.

## 2. Kendaraan liar

`WildVeh_Tick` (20 detik) menjaga maksimal **5 kendaraan liar per lobby**.

- Titik spawn acak di zona non-kamp, dijatuhkan ke tanah dengan ColAndreas.
- Ditolak bila < 22 m dari pemain atau < 7 m dari kendaraan lain.
- Nomor plat `ABANDON`, model acak dari daftar kendaraan jalan.
- Bensin awal 10–45% tangki, **45% terkunci**.
- Bensin dihitung dari jarak, bukan odometer client:
  `liter = km × L/100km ÷ 100 × 80`. Skala 80 membuat satu tangki bertahan
  ±10–20 menit mengemudi, bukan berjam-jam.
- Tangki 0 → mesin mati, tidak bisa dinyalakan sampai diisi.
- Kendaraan hancur langsung dihapus dan slot pool dibebaskan.

## 3. Tangki per kategori

| Kategori | Contoh | Tangki | Konsumsi |
|---|---|---|---|
| Motor | Sanchez, Freeway, Faggio | 14 L | 4.5 L/100 km |
| Sedan/kompak | Esperanto, Sultan, Infernus | 60–65 L | 9–19 L/100 km |
| SUV/pickup | Bobcat, Rancher | 70–80 L | 12–13.5 L/100 km |
| Van/bus | Burrito, Boxville | 80 L | 13.5 L/100 km |
| Truk/industri | Benson, DFT-30 | 150 L | 28 L/100 km |

HUD kanan bawah: `FUEL 34.2/60 L` dan `LOCKED` hanya saat mengemudi.

## 4. Isi bensin

| Aksi | Lokasi | Biaya |
|---|---|---|
| `/isi [liter]` atau Y di pom | hanya di **Pom Bensin** kamp | $5/liter |
| `/isi kan` | pom, mengisi jerrycan kosong | $5/liter |
| Jerrycan dari tas | di mana saja, kendaraan ≤4.5 m | 1 jerrycan (20 L penuh) |
| `/sedot` | kendaraan terbengkalai ≤4.5 m | gratis, 25% memicu panas zona |

Jerrycan menyimpan isi dalam durability: `100% = 20 L`. Menuangkan ke tangki
mengurangi isi sesuai liter yang masuk; kaleng kosong tetap ada dan bisa diisi
ulang di pom. Bensin tidak pernah habis saat idle. Isi bahan bakar tidak
menyalakan mesin; pemain memakai `/mesin`.

## 4b. Perbaikan kendaraan

- Kondisi mobil liar 520–950 HP; di bawah 350 HP mesin bisa mati mendadak saat
  dikemudikan (peringatan `/perbaiki`).
- `/perbaiki` butuh **Repair Kit + Scrap 3**, lalu health kembali 1000.
- Repair Kit: pedagang $350, craft Scrap 3 + Electronics 1, atau loot peti/langka.
- Perbaikan bisa dilakukan di lapangan; tidak harus di garasi.

## 4c. Generator base

- Generator adalah struktur ke-7 di `/build`: Scrap 10 + Electronics 6, tier 2,
  maksimal satu per base, 1200 HP.
- Isi bahan bakar dengan `/generator` sambil membawa jerrycan dan berdiri ≤6 m
  dari generator. Satu jerrycan penuh = 20 L; tangki generator juga 20 L.
- Generator hanya membakar BBM saat **malam**. Konsumsi 0.06 L per tick 2 detik
  (±0.03 L/detik); satu jerrycan bertahan sekitar 11 menit malam.
- Selama generator berdaya, turret base tetap menembak pada malam. Tanpa daya,
  turret berhenti sampai pagi atau sampai BBM diisi.
- BBM disimpan di `base_structures.fuel`, jadi tidak hilang saat relog/restart.
  Bila BBM habis, pemilik base mendapat toast `GENSET MATI`.

## 5. Garasi dan kepemilikan

- `/garasi` hanya bisa dibuka di garasi kamp. `Y` di hotspot garasi juga membuka.
- Garasi menampung **4 kendaraan**. Slot kosong bisa diisi lewat "Beli kendaraan".
- Harga: Esperanto bekas $420, Sanchez $1800, Bobcat $3200, Burrito $3800,
  Sultan $4500. Pembelian dipotong dari uang karakter.
- "Keluarkan" menaruh mobil di salah satu dari 4 slot parkir, memakai bensin dan
  kondisi yang tersimpan. Tidak lagi memakai `%`; semua dalam liter.
- `/simpan` menyimpan kendaraan yang sedang dikemudikan atau yang terdekat
  (≤12 m) ke garasi. Bensin, health, dan status kunci masuk `owned_vehicles`.
- Pemain keluar server saat mengemudi mobil sendiri → mobil otomatis disimpan.
- Mobil hancur → diasuransikan: kembali ke garasi dengan health 400, bensin tetap.
- Mobil milik orang lain terkunci. Pemilik membuka dengan `/kunci`; pemain lain
  harus `/lockpick` (65%, lockpick habis, tidak ada refund).
- `/kunci` hanya mengunci untuk kendaraan liar. Untuk mobil sendiri, pemilik bisa
  mengunci dan membuka.

## 6. Item baru

| Item | id | Model | Berat | Sumber |
|---|---|---|---|---|
| Lockpick | 19 | 19574 | 0.1 | pedagang $150, craft (Scrap 2 + Electronics 1), loot langka |
| Jerrycan | 20 | 1650 | 4.0 | pedagang $90, peti darurat |
| Zombie Specimen | 21 | 1240 | 0.5 | 14% zombie biasa (pity: dijamin setelah 5 kill tanpa sampel), 45% mayat Brute/Stalker |
| Repair Kit | 22 | 19921 | 3.0 | pedagang $350, craft Scrap 3 + Electronics 1, loot |

Sampel zombie **tidak bisa disimpan di stash/peti party** dan tidak dijual ke pedagang.
Satu-satunya kegunaan adalah diserahkan di titik ekstraksi.
Sampel TIDAK jatuh bila pembunuh berdiri di zona aman (hub/shelter) atau kill murni NPC.

## 6b. Ekstraksi berbasis sampel

Titik ekstraksi di Camp Outskirts (`/ekstrak` atau Y).

- Minimum **3 sampel**, maksimum 8 sampel per penyerahan.
- Bayaran `$120/sampel + $100 bonus` → 3 sampel = $460, 8 sampel = $1060.
- Heal `20 + 6/sampel`, cure infeksi `10 + 4/sampel`.
- Cooldown **90 detik per lobby**; pesan menyebut sisa detik.
- Heli ekstraksi menambah panas zona (radius 14 m), jadi ekstraksi berulang
  memancing horde.
- Membawa sampel menambah target zombie Director hingga +4 per lobby.
- Setelah serah terima pemain dipindahkan ke spawn kamp dan sample anticheat
  dibersihkan. Tidak ada payout tanpa sampel.

## 6c. Luka terbuka

- Serangan zombie punya peluang menambah luka: Walker/Crawler 24%, Stalker 42%,
  Brute 55% (25% di antaranya langsung 2 stack), Bloater 32% + ledakan 50%.
- Maksimal **3 stack**. Setiap stack menyebabkan 0.9 damage per tick 5 detik
  (3 stack = 2.7 damage/5 dtk) dan tidak hilang sendiri.
- Bandage menutup **1** luka; Medkit menutup **semua** luka sekaligus.
- Luka disimpan di `characters.wounds`; mati/respawn membersihkannya, logout tidak.
- HUD menampilkan `LUKA n` di baris progres.

## 6d. Escort trader

- `/escort` hanya bisa dimulai **di luar kamp**, saat tidak ada escort lain di
  lobby, dan saat cooldown lobby sudah habis.
- Trader FCNPC muncul di samping pemain, mengikuti, dan harus diantar kembali ke
  kamp. Zombie dalam radius 3 m melukai trader 16 HP per tick; pemain juga bisa
  menembaknya.
- Gagal bila trader mati, pengantar mati, jarak >45 m, atau waktu >8 menit.
  Cooldown gagal 3 menit.
- Berhasil bila pengantar dan trader berada di zona kamp dalam 12 m. Hadiah:
  $750, 8 scrap, 2 medkit, 40 XP, dan 25% blueprint suppressor. Cooldown 10 menit
  per lobby.
- Escort menambah panas zona secara berkala, sehingga misi ini bukan jalan aman.

## 7. Uji

1. HUD tiga meter turun; makan/minum menaikkan; relog mempertahankan nilai.
2. `/kencing` pada PIPIS ≥12% → 0.
3. Tunggu spawn liar → mobil `ABANDON` → `/lockpick` → `/mesin`.
4. Mengemudi → `FUEL x/60 L` turun; habis → mesin mati; isi di pom atau jerrycan.
5. `/garasi` → beli Esperanto → keluarkan → `/simpan` → relog → bensin sama.
6. Hancurkan mobil sendiri → pesan asuransi → cek garasi health 400.
7. Bunuh zombie sampai HUD `SAMPEL 3` → `/ekstrak` → dibayar $460 dan
   dipindah ke spawn; ulangi dalam 90 detik → ditolak dengan sisa detik.
8. `/ekstrak` dengan 2 sampel → ditolak; coba simpan sampel di stash → tidak ada
   baris specimen di daftar deposit.
9. Rusakkan mobil liar sampai <350 HP → mesin mati → `/perbaiki` dengan kit +
   Scrap 3 → health 1000.
10. Isi jerrycan kosong dengan `/isi kan` di pom → pakai di mobil → isi berkurang;
    `/sedot` dari mobil liar mengisi jerrycan dan mengurangi bensin mobil.
11. Biarkan zombie memukul sampai HUD `LUKA 1+` → darah turun tiap 5 detik;
    pakai bandage → satu luka hilang; medkit → semua luka hilang.
12. Bangun Generator (tier 2), `/generator` dengan jerrycan, tunggu malam →
    turret tetap menembak; biarkan BBM habis → turret berhenti dan toast muncul.
13. Di luar kamp, `/escort` → trader mengikuti → antar ke kamp → hadiah masuk;
    ulangi langsung → ditolak dengan sisa cooldown.
