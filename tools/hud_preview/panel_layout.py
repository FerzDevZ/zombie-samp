#!/usr/bin/env python3
"""Gate geometri panel bangun base (/bangun).

Masalah nyata yang dijaga gate ini: pada skala 80% tombol LANGKAH mendarat
tepat di atas PASANG/BATAL sehingga label BATAL bertumpuk (terbukti pada
screenshot pemain). Penyebabnya: pasangY/hintY dihitung dari literal yang
diskalakan, sedangkan baris parameter SELALU mulai di y+40 dan tidak ikut
skala.

Skrip ini meniru BuildEdit_ApplyScale + BuildEdit_CreatePanel baris per baris
dan gagal bila ada kotak yang tumpang tindih, keluar panel, atau tombol yang
terlalu kecil untuk disentuh jari.
"""

import sys

SCALES = [1.00, 0.80, 0.62, 0.48]

# Basis layout: (panjang, lebar) sama dengan BuildEdit_ComputeLayout.
BASE = {
    # gaya HUD PENUH / MINIMAL (PC)
    "pc": dict(w=294.0, h=210.0, btnW=40.0, btnH=12.0, rowStep=16.0, infoStep=16.0,
               colL=186.0, colR=228.0, pasangX=12.0, pasangW=130.0, pasangY=142.0,
               pasangH=20.0, hintY=168.0, fontRow=0.19,
               min_btn_h=12.0, min_btn_w=30.0, min_text_w=104.0, min_h=140.0),
    # gaya HUD MOBILE KOMPAK
    "mobile": dict(w=206.0, h=224.0, btnW=38.0, btnH=16.0, rowStep=22.0, infoStep=13.0,
                   colL=116.0, colR=160.0, pasangX=10.0, pasangW=90.0, pasangY=170.0,
                   pasangH=22.0, hintY=200.0, fontRow=0.13,
                   min_btn_h=16.0, min_btn_w=34.0, min_text_w=84.0, min_h=132.0),
}

ROWS_TOP = 40.0   # judul y+7 dan divider y+34 tidak diskalakan
TITLE_W = 11.0 * 0.22 * 22.0   # "PEMBANGUNAN" font 2, letter 0.22
ROW_COUNT = 6     # X-/X+ Y-/Y+ Z-/Z+ < > - + LANGKAH


def apply_scale(base, scale):
    """Cermin BuildEdit_ApplyScale (Pawn) baris per baris."""
    g = dict(base)
    gap = 6.0

    g["btnW"] = max(base["btnW"] * scale, base["min_btn_w"])
    g["btnH"] = max(base["btnH"] * scale, base["min_btn_h"])

    row_step = base["rowStep"] * scale
    if row_step < g["btnH"] + 3.0:
        row_step = g["btnH"] + 3.0
    g["rowStep"] = row_step
    g["infoStep"] = max(base["infoStep"] * scale, 10.0)

    two_btn = (g["btnW"] * 2.0) + gap
    text_w = max(base["min_text_w"] * scale, 70.0)

    g["pasangW"] = max(base["pasangW"] * scale, 64.0)
    g["pasangH"] = max(base["pasangH"] * scale, base["min_btn_h"])
    g["pasangX"] = base["pasangX"] * scale

    pair_w = (g["pasangW"] * 2.0) + (g["pasangX"] * 2.0) + gap
    g["w"] = max(text_w + two_btn + 10.0, pair_w)

    g["colL"] = g["w"] - two_btn - 4.0
    g["colR"] = g["colL"] + g["btnW"] + gap

    rows_bottom = ROWS_TOP + (5.0 * g["rowStep"]) + g["btnH"]
    g["pasangY"] = rows_bottom + 8.0
    g["hintY"] = g["pasangY"] + g["pasangH"] + 5.0
    g["h"] = max(g["hintY"] + 12.0, base["min_h"])

    g["fontRow"] = max(base["fontRow"] * scale, 0.10)
    g["scale"] = scale
    return g


def boxes(g):
    """Semua kotak yang digambar panel, koordinat lokal (0,0 = sudut panel)."""
    out = {
        "judul":   (12.0, 7.0, TITLE_W, 11.0),
        "X":       (g["w"] - 26.0, 5.0, 20.0, 13.0),
        "UKURAN":  (g["w"] - 68.0, 5.0, 38.0, 13.0),
        "divider": (12.0, 34.0, g["w"] - 24.0, 1.0),
    }
    for i in range(ROW_COUNT):
        y = ROWS_TOP + (i * g["rowStep"])
        out[f"row{i}_kiri"] = (g["colL"], y, g["btnW"], g["btnH"])
        out[f"row{i}_kanan"] = (g["colR"], y, g["btnW"], g["btnH"])
    out["PASANG"] = (g["pasangX"], g["pasangY"], g["pasangW"], g["pasangH"])
    out["BATAL"] = (g["w"] - g["pasangW"] - g["pasangX"], g["pasangY"], g["pasangW"], g["pasangH"])
    out["hint"] = (12.0, g["hintY"], g["w"] - 24.0, 10.0)
    return out


def overlap(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return (ax < bx + bw) and (bx < ax + aw) and (ay < by + bh) and (by < ay + ah)


def check(g, label):
    problems = []
    boxes_map = boxes(g)

    # 1. tidak ada dua kotak yang bertumpuk (kecuali pasangan yang memang
    #    satu baris: tombol kiri vs kanan tidak pernah bertumpuk secara desain)
    items = list(boxes_map.items())
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            name_a, box_a = items[i]
            name_b, box_b = items[j]
            if overlap(box_a, box_b):
                problems.append(f"{name_a} menumpuk {name_b}")

    # 2. semua kotak di dalam panel
    for name, (x, y, w, h) in boxes_map.items():
        if x < 0.0 or y < 0.0:
            problems.append(f"{name} keluar panel (x={x:.1f} y={y:.1f})")
        if x + w > g["w"] + 0.01:
            problems.append(f"{name} lewat tepi kanan ({x + w:.1f} > {g['w']:.1f})")
        if y + h > g["h"] + 0.01:
            problems.append(f"{name} lewat tepi bawah ({y + h:.1f} > {g['h']:.1f})")

    # 2b. judul tidak boleh menabrak tombol UKURAN
    judul = boxes_map["judul"]
    ukuran = boxes_map["UKURAN"]
    if judul[0] + judul[2] + 4.0 > ukuran[0]:
        problems.append(f"judul ({judul[0] + judul[2]:.1f}) menyentuh UKURAN ({ukuran[0]:.1f})")

    # 3. target sentuh
    for name in ("PASANG", "BATAL"):
        if boxes_map[name][3] < g["min_btn_h"]:
            problems.append(f"{name} terlalu pendek untuk jari ({boxes_map[name][3]:.1f})")
    if g["btnH"] < g["min_btn_h"]:
        problems.append(f"tombol baris {g['btnH']:.1f} < {g['min_btn_h']} px")
    if g["btnW"] < g["min_btn_w"]:
        problems.append(f"lebar tombol {g['btnW']:.1f} < {g['min_btn_w']} px")
    if g["fontRow"] < 0.10:
        problems.append(f"font baris {g['fontRow']:.2f} < 0.10")

    status = "OK  " if not problems else "FAIL"
    print(f"[{status}] {label}: W={g['w']:6.1f} H={g['h']:6.1f} "
          f"rowStep={g['rowStep']:5.1f} btn={g['btnW']:.0f}x{g['btnH']:.0f} "
          f"PASANG y={g['pasangY']:6.1f} hint y={g['hintY']:6.1f}")
    for problem in problems:
        print(f"        -> {problem}")
    return problems


def render():
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("[INFO] PIL tidak ada, render dilewati")
        return None

    scale_px = 1.7
    pad = 24
    panels = []
    for style in ("pc", "mobile"):
        for scale in SCALES:
            panels.append((style, apply_scale(BASE[style], scale)))

    width = int(max(g["w"] for _, g in panels) * scale_px * len(SCALES) + pad * (len(SCALES) + 1))
    height = int(max(g["h"] for _, g in panels) * scale_px * 2 + pad * 3 + 20)
    img = Image.new("RGB", (width, height), (12, 16, 24))
    draw = ImageDraw.Draw(img)

    for index, (style, g) in enumerate(panels):
        row = 0 if style == "pc" else 1
        col = index % len(SCALES)
        ox = pad + col * (max(p["w"] for _, p in panels) * scale_px + pad)
        oy = pad + row * (max(p["h"] for _, p in panels) * scale_px + pad + 20)

        draw.rectangle([ox, oy, ox + g["w"] * scale_px, oy + g["h"] * scale_px],
                       fill=(22, 28, 38), outline=(70, 82, 100))
        for name, (x, y, w, h) in boxes(g).items():
            color = (58, 68, 84)
            if name in ("PASANG", "BATAL"):
                color = (170, 60, 60)
            elif name in ("X", "UKURAN"):
                color = (40, 48, 60)
            draw.rectangle([ox + x * scale_px, oy + y * scale_px,
                            ox + (x + w) * scale_px, oy + (y + h) * scale_px],
                           fill=color)
        draw.text((ox + 4, oy - 14), f"{style} {int(g['scale'] * 100)}%",
                  fill=(200, 210, 225))

    img.save("docs/_debug/panel_scale.png")
    print("[OK  ] render docs/_debug/panel_scale.png")
    return True


def main():
    failures = 0
    for style in ("pc", "mobile"):
        for scale in SCALES:
            g = apply_scale(BASE[style], scale)
            failures += len(check(g, f"{style} {int(scale * 100)}%"))

    render()
    if failures:
        print(f"GATE PANEL: GAGAL — {failures} masalah tata letak")
        return 1
    print(f"GATE PANEL: lulus — {len(SCALES) * 2} kombinasi (2 gaya x 4 skala), 0 tumpang tindih")
    return 0


if __name__ == "__main__":
    sys.exit(main())
