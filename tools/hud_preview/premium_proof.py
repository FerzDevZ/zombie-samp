#!/usr/bin/env python3
"""Renderer TextDraw dari export iTD/TDEditor (sprite + teks), untuk membuktikan
apa sebenarnya yang membuat UI server lain terlihat "mahal".

Dipakai untuk dua hal:
  1. membedah source milik orang lain:  python3 premium_proof.py <file.txt>
  2. memeriksa desain kita SEBELUM masuk Pawn (tidak perlu masuk server).

Aturan engine yang dipakai renderer ini (sesuai samp-textdraw-ui-craft):
  - font 4/5  : argumen "teks" adalah NAMA SPRITE, ukuran dari TextSize
  - font 0-3  : teks bitmap, ukuran dari LetterSize, TextSize = kotak wrap
  - warna     : AARRGGBB, alpha ikut dihitung (bertumpuk = makin pekat)
"""

import re
import sys

from PIL import Image, ImageDraw, ImageFont

SCALE = 3
CANVAS = (640, 480)
FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

BLOCK_RE = re.compile(
    r'CreatePlayerTextDraw\(playerid,\s*([0-9.]+),\s*([0-9.]+),\s*"([^"]*)"\);(.*?)(?=\n\s*\w+_\d+_\[playerid\] = CreatePlayerTextDraw|\Z)',
    re.S,
)


def field(body, name, cast=float, default=None):
    match = re.search(rf'{name}\(playerid, [^,]+, ([^)]+)\)', body)
    if not match:
        return default
    raw = match.group(1).strip()
    try:
        return cast(raw)
    except ValueError:
        return default


def parse(path):
    source = open(path, encoding="utf-8", errors="ignore").read()
    items = []
    for x, y, text, body in BLOCK_RE.findall(source):
        size_x = field(body, "PlayerTextDrawTextSize", float, 0.0)
        size_y = field(body, "PlayerTextDrawTextSize", float, 0.0)
        # TextSize punya dua argumen; ambil yang kedua juga
        pair = re.search(r'PlayerTextDrawTextSize\(playerid, [^,]+, ([0-9.]+), ([0-9.]+)\)', body)
        if pair:
            size_x, size_y = float(pair.group(1)), float(pair.group(2))
        items.append(dict(
            x=float(x), y=float(y), text=text,
            w=size_x, h=size_y,
            color=field(body, "PlayerTextDrawColor", int, -1),
            font=field(body, "PlayerTextDrawFont", int, 1),
            letterx=field(body, "PlayerTextDrawLetterSize", float, 0.0),
            outl=field(body, "PlayerTextDrawSetOutline", int, 0),
        ))
    return items


def argb(value):
    v = value & 0xFFFFFFFF
    return ((v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF)


def draw_items(path, out_path, title):
    items = parse(path)
    img = Image.new("RGBA", (CANVAS[0] * SCALE, CANVAS[1] * SCALE), (26, 34, 48, 255))
    draw = ImageDraw.Draw(img)

    for item in items:
        a, r, g, b = argb(item["color"])
        x, y = item["x"] * SCALE, item["y"] * SCALE
        w, h = item["w"] * SCALE, item["h"] * SCALE

        if item["font"] in (4, 5):
            # sprite: LD_BUM:blkdot = gumpalan padat, LD_BEAT:chit = bulatan
            layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
            layer_draw = ImageDraw.Draw(layer)
            if "chit" in item["text"]:
                layer_draw.ellipse([x, y, x + w, y + h], fill=(r, g, b, a))
            else:
                radius = max(1, min(w, h) * 0.42)
                layer_draw.rounded_rectangle([x, y, x + w, y + h], radius=radius, fill=(r, g, b, a))
            img.alpha_composite(layer)
        else:
            size = max(6, int(item["letterx"] * SCALE * 62))
            font = ImageFont.truetype(FONT_PATH, size)
            mask = Image.new("L", img.size, 0)
            ImageDraw.Draw(mask).text((x, y), item["text"], font=font, fill=a)
            tint = Image.new("RGBA", img.size, (r, g, b, 255))
            img.paste(tint, (0, 0), mask)

    header = ImageDraw.Draw(img)
    header.text((24, 12), title, font=ImageFont.truetype(FONT_PATH, 26), fill=(230, 238, 250))
    header.text((24, 44), f"{len(items)} primitif PlayerTextDraw", font=ImageFont.truetype(FONT_PATH, 20),
                fill=(150, 165, 188))
    img.convert("RGB").save(out_path)
    return len(items)


if __name__ == "__main__":
    source = sys.argv[1] if len(sys.argv) > 1 else "docs/_debug/ref_source.txt"
    target = sys.argv[2] if len(sys.argv) > 2 else "docs/_debug/ref_render.png"
    count = draw_items(source, target, "Rekonstruksi dari source: 2 sprite GTA + font 1 tanpa outline")
    print(f"[OK] {target} — {count} primitif")
