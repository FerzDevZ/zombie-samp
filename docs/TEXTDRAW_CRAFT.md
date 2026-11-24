# Kenapa TextDraw orang lain terlihat "mahal" — hasil bedah source

Bukti diambil dari satu panel radio (walkie-talkie) milik server lain yang
source-nya dikirim pemain (`docs/_debug/ref_source.txt`, screenshot
`docs/_debug/ref_td2.png`). Isinya 47 PlayerTextDraw.

## Angka yang menentukan

| Fakta | Nilai | Artinya |
|---|---|---|
| Jumlah primitif | **47** untuk satu panel kecil | detail = kuantitas, bukan trik |
| Sprite dipakai | **2**: `LD_BUM:blkdot`, `LD_BEAT:chit` | tekstur GTA bawaan, tidak ada custom model |
| Pemakaian sprite | 29 `blkdot` + 16 `chit` | `blkdot` direntang jadi panel/baris, `chit` jadi sudut bulat & knop |
| Font | **45 x font 4** (sprite), **2 x font 1** (teks) | ikon dibuat dari bentuk, hanya 2 elemen teks |
| Outline / shadow | **0 di 47 primitif** | tidak ada garis hitam = terlihat modern |
| LetterSize teks | `0.1825 x 0.9300`, `0.1890 x 0.9803` | huruf tipis dan gepeng, bukan tebal |
| Warna | cuma **3 nilai**, dua di antaranya alpha **11%** | `0x1C1C1CFF` @28, `0x90FFFF` @30, putih `-1` |
| Ukuran primitif | terkecil **0.937 x 1.167** (1 unit) | garis tipis dibuat dari blok 1-unit |

## Trik sebenarnya: alpha ditumpuk

Kedua warna non-putih ber-alpha 11%. Satu lapis tidak terlihat; ditumpuk
berlapis-lapis menjadi pekat dan halus:

```
 1 lapis  -> 11%    8 lapis -> 61%    24 lapis -> 94%
```

Itulah sebabnya sisi panel terlihat seperti kaca/glow bertingkat, bukan kotak
solid: setiap "gradien" di UI itu adalah beberapa blok 11% yang saling
menimpa pada offset berbeda. `PlayerTextDrawBoxColor` tidak bisa gradien —
tumpukan alpha bisa.

## Kenapa punya kita terlihat lebih kasar

| Aspek | Referensi | Kita sekarang |
|---|---|---|
| Permukaan | sprite `blkdot` + `chit` (sudut bulat, bisa transparan) | `UseBox=1` untuk beberapa kartu (kotak tajam) |
| Techteks | outline 0, shadow 0 | outline 1 + shadow (tebal/klasik) |
| Ukuran huruf | 0.18 x 0.93 | 0.20–0.34 (lebih besar, lebih "berat") |
| Warna | alpha 11% ditumpuk | mayoritas solid penuh |
| Detail ikon | digambar dari 3–8 primitif per ikon | ikon = karakter teks/spasi |

## Batas yang harus dijaga

Source di atas memakai `PlayerTextDrawTextSize(…, 1327.118, 1280.000)` pada
teks font 1 — pola yang **pecah di Android** (kotak wrap raksasa) dan sudah
dilarang di repo ini (`tools/hud_preview/generate.py`). Jadi meniru gaya ini
harus tetap: teks font 0–3 **tanpa** TextSize, dan jumlah primitif dijaga
karena 47 TD untuk satu panel berarti 5 panel sudah 235 dari 256 slot
PlayerTextDraw per pemain.

## Cara memakainya di repo ini

1. Rancang di renderer dulu (tanpa masuk server):
   `python3 tools/hud_preview/premium_proof.py <export.txt> docs/_debug/hasil.png`
   Renderer ini membaca export iTD/TDEditor (font 4 = sprite, font 0–3 = teks,
   alpha dihitung) dan menggambar hasilnya.
2. Pindahkan geometri ke Pawn lewat `TD_Sprite`/`TD_RoundedCard`
   (`.agents/skills/samp-textdraw-ui-craft/resources/samp_td_kit.inc`), bukan
   tulis tangan.
3. Verifikasi: `freepi-power samp td-preview`, `tools/hud_preview/panel_layout.py`,
   `tools/hud_preview/android_zone.py`, dual-target compile.
