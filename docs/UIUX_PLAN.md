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
