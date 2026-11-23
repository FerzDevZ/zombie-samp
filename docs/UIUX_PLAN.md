# UI/UX PLAN — HUD v4 (Mobile + 3 Gaya)

Tanggal: 2026-11-23. Status: DISETUJUI UNTUK DIEKSEKUSI.

## 1. Temuan investigasi (bukti, bukan tebakan)

| Keluhan | Investigasi | Kesimpulan |
|---|---|---|
| SCRAP/SAMPEL selalu 0 | `HUD_Update` (hud.inc:697) tanpa early-return mencapai baris loadout tiap 500 ms; `Inventory_CountItem` menghitung `PlayerInventory` yang sama dengan `/inv`; DB: char 8 (s1) = 7 scrap, char 12 (s2) = 0, stash/party_stash kosong, kolom legacy `characters.scrap` = 0 | Kode HUD terbukti benar; **tidak ada scrap 50 di database**. Nilai 0 = isi tas memang 0 (kemungkinan main di char s2 yang kosong). Tindakan: log count saat inventory dimuat (`[INV] scrap=%d sampel=%d`) supaya laporan "0 padahal ada" bisa dibuktikan/kidalahkan di log, bukan tebakan |
| HUD kebesaran di Android | Layar virtual SA-MP 640x480 di-stretch ke layar HP kecil; kartu HUD kiri-bawah (y 288–388) **menimpa zona joystick virtual** (x 0–165, y 310–480) | Perlu skala + relokasi otomatis untuk klien mobile |
| BBM "gajelas" | Baris BBM tidak menampilkan persen; bar terpisah dari angka tanpa hubungan visual | Format baru: `BBM 62%  62/100L 240km` — persen, liter, jarak dalam satu baris; bar langsung di sebelah kanan angka |

## 2. Tiga gaya (dipilih via `/hudgaya 1|2|3`, otomatis untuk mobile)

| # | Nama | Skala | Karakter | Target |
|---|---|---|---|---|
| 1 | **Penuh** | 1.00 | Layout v3 sekarang: kartu kaca kiri-bawah, bar bersegmen, kotak kanan | PC desktop |
| 2 | **Mobile Kompak** | 0.70 | Semua koordinat diskalakan 0.70 menuju pojok kiri-ATAS → kartu keluar dari zona joystick (y 288→202) dan tombol sentuh kanan-bawah; elemen center (boss/raid/prompt) tetap di tengah horizontal via `320-(320-x)*S`; deteksi otomatis: `GetPlayerVersion` mengandung "mobile" | Android |
| 3 | **Minimalis** | 0.85 | Tanpa kartu & tanpa kotak (UseBox 0): hanya bar + angka + label, paling ringan | PC low-end / pemain yang mau bersih |

Aturan tetap: layar 640x480; teks terburuk tetap diuji (gate `gHudFitRows` + `generate.py`); bar bersegmen; dirty-flag 500 ms; setiap gaya dirender di galeri (`11-hud-mobile`, `12-hud-minimal`) dan diverifikasi vision model.

## 3. BBM diperjelas

- `BBM 100%  100/100L 8000km` (25–26 char) — worst case aman ≤ 628 px.
- BBM ≤25%: `BBM 12%  12/80L pom 240m BD` (27 char) → gate baris HudVehicle 26→27.
- GENSET: `GENSET 60%  12/20L` (18 char).
- Bar tetap hilang utuh saat ≤25% / genset mati (invarian bar-kondisional).

## 4. SCRAP/SAMPEL

- Format chip tetap `SCRAP %d  SAMPEL %d` (data sudah benar), tapi sekarang:
  - log saat inventory dimuat → bukti di server_log.txt;
  - jika HUD menampilkan 0 padahal `/inv` menampilkan > 0 → itu bug baru; lapor dengan log, jangan tebakan.

## 5. Gate & verifikasi

1. Build 0 warning 0 error.
2. Selftest 53/53 PASS kedua instance.
3. `generate.py` GATE LULUS (WORST_TEXT HudVehicle diperbarui).
4. `gallery.py` 12 halaman, guard literal skala (`0.70`, `0.85`).
5. Vision check: halaman mobile tidak menimpa zona joystick/tombol sentuh.

## Kolom kendaraan v5 (commit ini)

| Baris | Isi | Sumber | Gate |
|---|---|---|---|
| Identitas | `MOBIL <model>  TERKUNCI/TERBUKA` | `GetVehicleModel` + `Vehicle_IsLocked` | worst-case 19 char, muat 264 px |
| Speedometer | angka besar font 2 + `km/j` + bar KMH | `GetVehicleVelocity` × 180, batas 240 | bar 264×4, warna ≥140 kuning, 240 merah |
| Bahan bakar | `FUEL nn%  n/nnL pom`. . . + bar | `Vehicle_GetFuel` | bar tetap tampil di semua gaya |
| GPS | `GPS 234m TL  <nama>` + bar progres | `Gps_*` (`gps.inc`) | nama dipotong 12 char, arah dari tabel 8 slot |

- Dua kartu: `y 258..322` (info/speed/FUEL) dan `y 323..345` (GPS) — keluar dari mobil tidak meninggalkan kartu kosong.
- Gaya: PENUH & MOBILE = kartu + bar KMH + bar GPS; MINIMAL = teks + bar FUEL saja (bar KMH/GPS dan kartu hilang, **bar FUEL tidak pernah hilang**).
- Per-modul bisa dimatikan di `/settings` (butir 9–11) lewat `characters.pref_hud_modul`.
- `/gps` membuka TABLIST: Base saya / Airdrop aktif / Rekan party / Matikan; tiba otomatis di bawah 20 m.

## Design HUD (skin) + `/gps` yang selalu punya tujuan — v6

### Kenapa ditambah
Pemain meminta **memilih rupa textdraw** HUD, bukan hanya gayanya. Sebelumnya
`/settings` hanya punya "Gaya HUD" (PENUH / MOBILE / MINIMAL) dan baris modul;
tidak ada satu pun baris yang mengubah **palet** HUD. `/gps` juga bisa kosong
tanpa tujuan sehingga penunjuk arah tidak pernah muncul.

### Struktur menu `/settings` sekarang (urut dari atas)
| Baris | Isi | Efek |
|---|---|---|
| 0 | Gaya HUD (auto / penuh / mobile / minimal) | permukaan + bar per grup |
| 1 | **Design HUD** (auto / KACA / NEON / TACTICAL / TERANG) | palet kartu, warna teks, warna aksen, bar solid vs segmen |
| 2..8 | detail, skala, kontras, darah, prompt, bahasa, sisi break-even | perilaku |
| 9 | mode renderer textdraw (auto / aman / cepat) | box vs sprite (Android) |
| 10..12 | baris MOBIL / speedometer / GPS | modul tampil atau tidak |

### Tabel design (`gHudSkins` di `hud.inc`)
| # | Nama | Kartu | Track | Teks utama | Aksen | Bar |
|---|---|---|---|---|---|---|
| 0 | AUTO | diresolusi saat apply: mobile → NEON, PC → KACA | | | | |
| 1 | KACA | `0B0F19E6` | `1E293BFF` | `F8FAFCFF` | `10B981FF` | segmen |
| 2 | NEON | `06141BF0` | `0E3A46FF` | `ECFEFFFF` | `22D3EEFF` | solid |
| 3 | TACTICAL | `1A1C14F2` | `33351FFF` | `F5F3E7FF` | `F59E0BFF` | solid |
| 4 | TERANG | `E8E4D9F7` | `BBB5A5FF` | `1F2937FF` | `0F766EFF` | solid |

Aturan yang tidak boleh dilanggar:
1. **Merah bahaya tidak pernah diubah design.** `HUD_SkinBarColor` hanya
   memetakan aksen netral (primary/info/warn); `UI_ACCENT_DANGER` lolos apa adanya
   supaya arti "HP kritis" tetap sama di semua design.
2. **Design diresolusi SEBELUM `HUD_Create`**, bukan di-recolour sesudahnya —
   kalau tidak, textdraw sudah terbuat dengan palet lama.
3. **Warna track dan warna celah bar diambil dari tabel skin**, sehingga bar
   segmen di design terang tidak menyisakan garis gelap.

### Grid kolom kendaraan (setelah dirapikan)
```
256 .. 320  kartu A : info 259 | angka speed 267 (letter 1.30) | km/j 275 @x396
                      bar KMH 286 | FUEL 293 | bar FUEL 305
322 .. 344  kartu B : GPS 325 | bar GPS 336
```
Semua baris teks memakai LetterSize Y `0.62` (satu garis dasar); hanya angka
kecepatan memakai `1.30`. `HUD_VEHC_CARD_Y` turun dari 258 → 256 supaya tidak
menempel baris PIP di `y 348`.

### `/gps` — daftar tujuan dibangun satu kali
`Gps_BuildList` mengisi tabel `gGpsKind / gGpsName / gGpsX/Y/Z` dan
`Gps_ShowMenu` **serta** `Gps_OnDialogResponse` memakai indeks yang sama. Dulu
keduanya menyusun baris sendiri-sendiri: ketika satu syarat berubah (base belum
dibangun) nomor baris bergeser dan pilihan "base" memasang target yang salah.

Urutan tujuan: **KAMP UTAMA (selalu ada)** → zona terdekat → base pribadi →
airdrop aktif → rekan party → rekan satu lobby → baris "Matikan GPS".
Kamp memakai `SPAWN_X/Y/Z` yang sama dengan lobby, jadi tidak ada salinan angka.
