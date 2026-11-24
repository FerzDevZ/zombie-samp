---
name: samp-android-safe-hud-layout
description: Ruang aman HUD/TextDraw untuk klien SA-MP Android 2340x1080 — geometri terukur (Y 2.400 px/unit, potong di y447), skala gaya berjangkar bawah, zona sentuh kanan, dan gate otomatis tools/hud_preview/android_zone.py. Pakai sebelum menambah atau memindah elemen HUD, atau saat pemain melaporkan HUD menutupi layar / kepotong / gaya tertentu berantakan.
---

# Ruang Aman HUD Android (SA-MP / open.mp)

Skill ini ada karena dua bug nyata di repo ini:

1. Blok BBM berada di tengah layar Android — saya salah menurunkan skala klien
   (1.70 px/unit), padahal yang benar 2.400 px/unit.
2. Setelah blok dipindah ke bawah, gaya MOBILE (0.70) dan MINIMAL (0.85) masih
   menariknya kembali ke tengah, karena `HUD_Y` = `y * scale` (jangkar atas).

## 1. Geometri klien (terukur, jangan ditawar)

Turunan dari screenshot 2340×1080: tepi kartu `design y328..428` tampak di
`px 795..1035`.

```
px_y = 2.400 * y + 8        kanvas 480 unit = 1160 px
layar 1080 px               -> terlihat hanya y <= 447, sisanya DIPOTONG
px_x = 3.106 * x            kanvas 1984 px, 356 px sisanya untuk tombol klien
```

| y (kanvas) | px | % tinggi layar |
|---|---|---|
| 328 | 795 | 73.6% |
| 396 | 958 | 88.7% |
| 424 | 1025 | 94.9% |
| 439 | 1061 | 98.3% |
| 447 | 1081 | 100% (batas) |

**Kesimpulan:** semua grup HUD hidup di `y 320..446`. Angka `y ≥ 448` adalah
kode mati di klien Android.

## 2. Skala gaya harus berjangkar BAWAH

```pawn
#define HUD_ANCHOR_Y 440.0
stock Float:HUD_Y(playerid, Float:y)  { return HUD_ANCHOR_Y - (HUD_ANCHOR_Y - y) * HUD_ScaleOf(playerid); }
stock Float:HUD_YT(playerid, Float:y) { return y * HUD_ScaleOf(playerid); }   // hanya elemen ATAS
```

| Gaya | Skala | kartuA | FUEL | bar BBM | bar GPS |
|---|---|---|---|---|---|
| PENUH | 1.00 | 396–434 | 424 | 431 | 443 |
| MOBILE | 0.70 | 409–436 | 429 | 434 | 442 |
| MINIMAL | 0.85 | 403–435 | 426 | 432 | 443 |

Kalau `HUD_Y` kembali memakai jangkar atas, ketiga baris di atas akan berakhir
di 277 / 337 — persis keluhan "gaya 2 dan 3 masih di tengah".

## 3. Zona sentuh klien

| Zona | x (kanvas) | y (kanvas) |
|---|---|---|
| joystick kiri | 10–40 | 452–499 (di luar layar) |
| action pad kanan | 497–750 | 312–499 |

HUD tidak boleh menaruh **elemen yang bisa disentuh** di area itu; teks pasif
boleh melewati asalkan tidak tumpang tindih secara visual dengan ikon.

## 4. Gate otomatis (wajib lulus sebelum commit)

```bash
python3 tools/hud_preview/android_zone.py              # gate zona
python3 tools/hud_preview/android_zone.py --self-test  # bukti gate menangkap bug
/tmp/hudvenv/bin/python tools/hud_preview/gaya_proof.py   # bukti visual 3 gaya
bash scripts/build.sh                                  # compile + dua gate
```

Gate memeriksa **setiap** `HUD_X`/`HUD_XC`/`HUD_Y`/`HUD_YT` di `hud.inc`,
mensimulasikan skala ketiga gaya, dan menolak build bila ada elemen di luar
`320..446` (atau `HUD_YT` di luar `0..130`).

## 5. Alur kerja saat pemain melapor "HUD nutupin layar"

1. Minta screenshot terbaru; jangan percaya build lama.
2. Turunkan skala dari dua titik acuan yang jelas.
3. Hitung batas terlihat; bandingkan dengan pita yang dipakai kode.
4. Perbaiki di sumber (`HUD_Y`/`#define`), bukan menambal elemen satu per satu.
5. Kunci dengan selftest + gate + MEMORY.md.
6. Laporkan tabel `elemen | lama | baru | % layar` beserta keluaran gate.
