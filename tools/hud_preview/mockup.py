"""Render mockup HUD 640x480 (diperbesar 3x) memakai geometri yang SAMA dengan
gamemodes/modules/ui/hud.inc. Dipakai untuk menilai tata letak secara visual
sebelum angka apa pun dipindahkan ke Pawn.

Jalankan: /tmp/hudvenv/bin/python tools/hud_preview/mockup.py out.png
"""
import sys
from PIL import Image, ImageDraw, ImageFont

SCALE = 3
W, H = 640, 480

# fonts.dat lebar karakter font 1 (sama dengan TDCharWidth di td_measure.inc)
FONT1 = {
    32: 15, 33: 9, 37: 34, 43: 20, 45: 14, 46: 12, 47: 15,
    48: 23, 49: 15, 50: 21, 51: 21, 52: 21, 53: 21, 54: 21, 55: 20,
    56: 21, 57: 21, 65: 22, 66: 19, 67: 19, 68: 22, 69: 16, 70: 19,
    71: 24, 72: 22, 73: 11, 74: 16, 75: 21, 76: 15, 77: 28, 78: 24,
    79: 27, 80: 20, 81: 25, 82: 19, 83: 19, 84: 18, 85: 23, 86: 23,
    87: 31, 88: 23, 89: 19, 90: 21, 95: 21, 97: 19, 98: 20, 99: 14,
    100: 20, 101: 19, 102: 13, 103: 20, 104: 19, 105: 9, 106: 9, 107: 19,
    108: 9, 109: 29, 110: 19, 111: 21, 112: 19, 113: 19, 114: 15, 115: 15,
    116: 14, 117: 18, 118: 19, 119: 27, 120: 20, 121: 20, 122: 17,
}


def text_width(text, letter_x):
    return sum(FONT1.get(ord(ch), 32 and FONT1[32]) for ch in text) * letter_x


def font_for(letter_y, letter_x=None):
    # tinggi glyph ~ letterY * 11 satuan pada kanvas 640x480; lebar ikut letterX
    px = max(6, int(round(letter_y * 11 * SCALE)))
    return _font(px)


def _font(px):
    try:
        return ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", px)
    except Exception:
        return ImageFont.load_default()


COMP = [1.0]


def rr(draw, box, radius, fill, outline=None):
    x0, y0, x1, y1 = [v * SCALE for v in box]
    x0 *= COMP[0]; x1 *= COMP[0]
    draw.rounded_rectangle([x0, y0, x1, y1], radius=radius * SCALE,
                           fill=fill, outline=outline)


def text(draw, x, y, s, letter_x, letter_y, color):
    """Teks digambar lalu DIPERAS horizontal ke lebar fonts.dat, supaya mockup
    memakai ukuran yang sama dengan TextDraw asli (bukan lebar font sistem)."""
    target_w = max(1, int(round(text_width(s, letter_x * COMP[0]) * SCALE)))
    f = font_for(letter_y)
    probe = Image.new("RGBA", (10, 10))
    box = ImageDraw.Draw(probe).textbbox((0, 0), s, font=f)
    w, h = box[2] - box[0], box[3] - box[1]
    if w <= 0 or h <= 0:
        return
    layer = Image.new("RGBA", (w + 4, h + 6), (0, 0, 0, 0))
    ImageDraw.Draw(layer).text((2 - box[0], 3 - box[1]), s, font=f, fill=color)
    layer = layer.resize((target_w, layer.height), Image.LANCZOS)
    draw._image.paste(layer, (int(x * SCALE * COMP[0]), int(y * SCALE)),
                      layer)


def bar(draw, x, y, w, h, fill_ratio, track=(38, 45, 55), fill=(16, 185, 129)):
    w = w * COMP[0]
    rr(draw, (x, y, x + w, y + h), h / 2, track + (255,))
    if fill_ratio > 0:
        rr(draw, (x, y, x + w * fill_ratio, y + h), h / 2, fill + (255,))


def render(path, design, comp=1.0, stretch=1.0):
    img = Image.new("RGB", (W * SCALE, H * SCALE), (26, 32, 24))
    d = ImageDraw.Draw(img, "RGBA")

    # dunia: jalan + rumput supaya kontras teks terlihat realistis
    d.rectangle([0, 0, W * SCALE, 210 * SCALE], fill=(70, 78, 60, 255))
    d.rectangle([0, 210 * SCALE, W * SCALE, H * SCALE], fill=(40, 44, 38, 255))
    for i in range(0, W, 40):
        d.line([i * SCALE, 210 * SCALE, (i + 60) * SCALE, 260 * SCALE],
               fill=(52, 56, 48, 255), width=2)

    p = design["panel"]
    rr(d, (p["x"], p["y"], p["x"] + p["w"], p["y"] + p["h"]), 3,
       (10, 14, 20, 205))
    rr(d, (p["x"] + 2, p["y"], p["x"] + p["w"] - 2, p["y"] + 1), 1,
       (16, 185, 129, 235))

    text(d, p["brand"][0], p["brand"][1], "LAST_STAND", 0.20, 0.85, (232, 238, 245))
    text(d, p["zone"][0], p["zone"][1], "Bandara Lama [WASPADA]", 0.15, 0.72,
         (150, 200, 230))

    for row in design["rows"]:
        text(d, row["name"][0], row["name"][1], row["label"], 0.16, 0.78, (200, 208, 218))
        bar(d, row["bar"][0], row["bar"][1], row["bar_w"], row["bar_h"], row["ratio"],
            fill=row["color"])
        text(d, row["value"][0], row["value"][1], row["text"], 0.16, 0.78, (232, 238, 245))

    for mini in design["minis"]:
        text(d, mini["name"][0], mini["name"][1], mini["label"], 0.15, 0.72, (200, 208, 218))
        bar(d, mini["bar"][0], mini["bar"][1], mini["bar_w"], mini["bar_h"], mini["ratio"])
        text(d, mini["value"][0], mini["value"][1], mini["text"], 0.15, 0.72, (232, 238, 245))

    text(d, p["progress"][0], p["progress"][1], "LVL 3  SP 2  LUKA 0  /skill",
         0.15, 0.72, (190, 198, 208))

    v = design["vehicle"]
    if v:
        rr(d, (v["x"], v["y"], v["x"] + v["w"], v["y"] + v["h"]), 2, (10, 14, 20, 205))
        text(d, v["text_x"], v["text_y"], "BBM 54%  43/80L 120km", 0.17, 0.70,
             (16, 185, 129))
        bar(d, v["bar_x"], v["bar_y"], v["bar_w"], 5, 0.54)

    # Simulasi klien HP: kanvas 640x480 diregangkan ke layar 20:9 (x 1.52x).
    if stretch != 1.0:
        img = img.resize((int(W * SCALE * stretch), H * SCALE), Image.LANCZOS)
    img.save(path)
    print("mockup:", path)


def run(comp, stretch, out):
    COMP[0] = comp
    render(out, DESIGN, comp=comp, stretch=stretch)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "mockup.png"
    DESIGN = {
        "panel": {
            "x": 12, "y": 300, "w": 284, "h": 84,
            "brand": (22, 303), "zone": (150, 305), "progress": (22, 368),
        },
        "rows": [
            {"label": "HP", "name": (22, 316), "bar": (56, 318), "bar_w": 100,
             "bar_h": 5, "ratio": 1.0, "color": (16, 185, 129),
             "value": (162, 316), "text": "100/100"},
            {"label": "ARM", "name": (22, 328), "bar": (56, 330), "bar_w": 100,
             "bar_h": 5, "ratio": 0.0, "color": (56, 132, 255),
             "value": (162, 328), "text": "0/100"},
            {"label": "INF", "name": (22, 340), "bar": (56, 342), "bar_w": 100,
             "bar_h": 5, "ratio": 0.0, "color": (16, 185, 129),
             "value": (162, 340), "text": "0%"},
        ],
        "minis": [
            {"label": "LAP", "name": (22, 354), "bar": (48, 356), "bar_w": 40,
             "bar_h": 4, "ratio": 0.82, "value": (92, 354), "text": "82"},
            {"label": "HAUS", "name": (108, 354), "bar": (134, 356), "bar_w": 40,
             "bar_h": 4, "ratio": 0.76, "value": (178, 354), "text": "76"},
            {"label": "PIP", "name": (196, 354), "bar": (222, 356), "bar_w": 40,
             "bar_h": 4, "ratio": 0.12, "value": (266, 354), "text": "12"},
        ],
        "vehicle": {"x": 336, "y": 330, "w": 292, "h": 20,
                    "text_x": 342, "text_y": 334,
                    "bar_x": 500, "bar_y": 338, "bar_w": 100},
    }
    comp = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
    stretch = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    run(comp, stretch, out)
