"""Bukti visual: blok BBM untuk KETIGA gaya HUD menempel di pita bawah.

Meniru hud.inc:  y' = 440 - (440 - y) * scale     (jangkar bawah)
scale: PENUH 1.00, MOBILE 0.70, MINIMAL 0.85.
Kanvas 640x480; klien Android memotong di y=447 (garis merah).
"""
from PIL import Image, ImageDraw

W, H, SCALE_PX = 640, 480, 3
ANCHOR = 440.0
Y_LIMIT = 447.0
GAYAS = [("PENUH", 1.00), ("MOBILE", 0.70), ("MINIMAL", 0.85)]

ROWS = [  # (label, y, kind)
    ("MOBIL 518 TERBUKA", 398.0, "text"),
    ("128 km/j", 404.0, "speed"),
    ("", 417.0, "bar"),
    ("FUEL 48%  33/70L  6km", 424.0, "text"),
    ("", 431.0, "bar"),
    ("GPS 234m TL KAMP UTAMA", 437.0, "text"),
    ("", 443.0, "bar"),
]


def ty(y, k):
    return ANCHOR - (ANCHOR - y) * k


def draw_panel(d, x0, k, title):
    d.rectangle([x0, 0, x0 + W - 1, H - 1], fill=(58, 62, 70))
    d.rectangle([x0 + 8, 8, x0 + 150, 26], fill=(20, 26, 34))
    d.text((x0 + 12, 12), f"{title}  (x{k:.2f})", fill=(230, 236, 245))

    # pita yang benar-benar terlihat di klien Android
    d.rectangle([x0, 0, x0 + W - 1, int(Y_LIMIT)], outline=(255, 90, 90))
    d.line([x0, Y_LIMIT, x0 + W - 1, Y_LIMIT], fill=(255, 90, 90))

    # kartu kiri
    top, bot = ty(328.0, k), ty(428.0, k)
    d.rounded_rectangle([x0 + 16, top, x0 + 304, bot], 3, fill=(11, 15, 25))
    d.text((x0 + 22, top + 6), "LAST STAND  Wasteland [AMAN]", fill=(148, 163, 184))
    d.text((x0 + 22, top + 30), "HP 93/100   ARM 0/100   INF 0%", fill=(232, 238, 245))
    d.text((x0 + 22, top + 60), "UANG $0        JAM 01:10", fill=(245, 158, 11))

    # kartu kendaraan A + B
    a_top, a_bot = ty(396.0, k), ty(434.0, k)
    b_top, b_bot = ty(435.0, k), ty(446.0, k)
    d.rounded_rectangle([x0 + 352, a_top, x0 + 628, a_bot], 3, fill=(11, 15, 25))
    d.rounded_rectangle([x0 + 352, b_top, x0 + 628, b_bot], 2, fill=(11, 15, 25))
    for label, y, kind in ROWS:
        yy = ty(y, k)
        if kind == "bar":
            d.rectangle([x0 + 356, yy, x0 + 620, yy + 3], fill=(16, 185, 129))
        elif kind == "speed":
            d.text((x0 + 356, yy - 2), label, fill=(16, 185, 129))
        else:
            d.text((x0 + 356, yy), label, fill=(248, 250, 252))

    d.text((x0 + 356, a_bot + 2), f"kartu A {a_top:.0f}..{a_bot:.0f}", fill=(255, 200, 120))


def main():
    img = Image.new("RGB", (W * len(GAYAS) * 1, H), (30, 34, 40))
    d = ImageDraw.Draw(img)
    for i, (name, k) in enumerate(GAYAS):
        draw_panel(d, i * W, k, name)
    img = img.resize((img.width * 1, img.height * 1))
    out = "docs/_debug/gaya_bawah.png"
    img.save(out)
    print("tulis", out)
    for name, k in GAYAS:
        print(f"{name:8s} x{k:.2f}  kartuA {ty(396,k):6.1f}..{ty(434,k):6.1f}  "
              f"FUEL {ty(424,k):6.1f}  barBBM {ty(431,k):6.1f}  "
              f"GPSbar {ty(443,k):6.1f}  kartuKiri {ty(328,k):6.1f}..{ty(428,k):6.1f}")


if __name__ == "__main__":
    main()
