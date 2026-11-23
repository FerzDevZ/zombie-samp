#!/usr/bin/env python3
"""Gate + preview tata letak HUD LAST STAND (640x480).

Tiga pemeriksaan, exit code 1 bila salah satu gagal:
  1. DRIFT      — angka x / LetterSize X di HUD_Create harus sama dengan tabel
                  gHudFitRows di file yang sama (tabel basi = gate palsu).
  2. PANJANG    — string terburuk di skrip ini harus sepanjang hudFitWorstLen.
  3. MUAT       — setiap baris harus berakhir sebelum tepi kanan 640 px.

Bukan emulator SA-MP: metrik glif diperkirakan (HUD_CHAR_RATIO), jadi yang
divalidasi GEOMETRI, bukan bentuk huruf persis di client.

Pakai: python3 tools/hud_preview/generate.py [keluaran.html]
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HUD = ROOT / "gamemodes/modules/ui/hud.inc"
CONST = ROOT / "gamemodes/modules/config/constants.inc"

SCREEN_W = 640.0

# lebar dasar karakter font 1 dari data/fonts.dat GTA San Andreas
# (dipublikasikan di forum SA-MP; dipakai kristoisberg/samp-td-string-width).
# Lebar teks pada kanvas 640x480 = jumlah(lebar karakter) * LetterSize X.
FONT1_WIDTH = [
    0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 9, 17, 27, 20, 34, 23, 12, 12, 12, 21, 20, 12, 14, 12, 15,
    23, 15, 21, 21, 21, 21, 21, 21, 20, 21, 12, 12, 24, 24, 24, 19,
    10, 22, 19, 19, 22, 16, 19, 24, 22, 11, 16, 21, 15, 28, 24, 27,
    20, 25, 19, 19, 18, 23, 23, 31, 23, 19, 21, 21, 13, 35, 11, 21,
    10, 19, 20, 14, 20, 19, 13, 20, 19, 9, 9, 19, 9, 29, 19, 21,
    19, 19, 15, 15, 14, 18, 19, 27, 20, 20, 17, 21, 17, 20, 15, 15,
    22, 22, 22, 22, 29, 19, 16, 16, 16, 16, 11, 11, 11, 11, 27, 27,
    27, 27, 23, 23, 23, 23, 20, 19, 19, 19, 19, 30, 14, 19, 19, 19,
    19, 9, 9, 9, 9, 21, 21, 21, 21, 18, 18, 18, 18, 24, 19, 19,
]


def measured_width(text: str, size_x: float) -> float:
    """Lebar nyata di kanvas 640x480 memakai tabel fonts.dat."""
    total = 0
    for ch in text:
        code = ord(ch)
        if code > 175:
            code = 32
        total += FONT1_WIDTH[code]
    return total * size_x
# Harus sama dengan HUD_CHAR_RATIO di hud.inc.
CHAR_ADVANCE_RATIO = 35.3
LINE_HEIGHT_RATIO = 10.5

# String TERBURUK per baris, bukan contoh pendek: nilai maksimum tiap format.
WORST_TEXT = {
    "HudBrand": "LAST_STAND",
    "HudZone": "Bandara Lama [WASPADA]",
    "HudHpName": "HP",
    "HudHpVal": "100/100",
    "HudArmName": "ARM",
    "HudArmVal": "100/100",
    "HudInfName": "INF",
    "HudInfVal": "100%",
    "HudLapName": "LAP",
    "HudLapVal": "100",
    "HudHausName": "HAUS",
    "HudHausVal": "100",
    "HudPipName": "PIP",
    "HudPipVal": "100",
    "HudProgress": "LVL 99  SP 999  LUKA 9  /skill",
    "HudLoadout": "SCRAP 9999  SAMPEL 999",
    "HudVehicle": "BBM 12%  12/80L pom 240m BD",
    "HudBoss": "ABOMINATION 100%",
    "HudRaid": "RAID KAMP 14 ZOMBIE  3:00",
    "HudPromptText": "Y  Ekstraksi sampel air",
}


def parse_colors(text: str) -> dict[str, str]:
    colors: dict[str, str] = {}
    for name, value in re.findall(r"#define\s+(UI_\w+)\s+0x([0-9A-Fa-f]{8})", text):
        colors[name] = "#" + value[:6].lower()
    return colors


DEFINES: dict[str, float] = {}


def parse_defines(text: str) -> None:
    """Konstanta geometri (mis. HUD_VEH_TEXT_X 356.0) supaya gate membaca
    angka yang sama dengan HUD_Create, bukan menebak dari argumen."""
    for name, value in re.findall(r"#define\s+(HUD_\w+)\s+(-?[\d.]+)", text):
        DEFINES[name] = float(value)


def _num(arg: str) -> float:
    """Ambil angka dari argumen koordinat: literal 26.0 atau HUD_X(playerid, 26.0).
    Pada skala 1.0 (PC) helper mengembalikan literal itu sendiri, jadi gate
    selalu menguji layout PC yang paling lebar."""
    token = re.findall(r"[A-Za-z_]\w*", arg)
    for name in reversed(token):
        if name in DEFINES:
            return DEFINES[name]
    found = re.findall(r"(-?\d+\.?\d*)", arg)
    return float(found[-1]) if found else 0.0


def _split_args(call: str) -> list[str]:
    """Pecah argumen pemanggilan pada koma tingkat atas (sadar tanda kurung).
    Perlu karena koordinat kini dibungkus HUD_X(playerid, 26.0)."""
    args, depth, current = [], 0, []
    for ch in call:
        if ch in "(\\[":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            args.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
    if current:
        args.append("".join(current).strip())
    return args


def parse_textdraws(text: str) -> list[dict]:
    draws: list[dict] = []
    create = re.compile(
        r"(Hud\w+)\[playerid\]\s*=\s*CreatePlayerTextDraw\(([^;]+)\);",
        re.S,
    )
    for match in create.finditer(text):
        var = match.group(1)
        args = _split_args(match.group(2))
        if len(args) < 4:
            continue
        x_arg, y_arg, initial = args[1], args[2], args[3].strip('"')
        # Sprite (LD_SPAC/LD_BEAT) dan TD berbasis array (kartu, bar) bukan teks:
        # tidak punya string terburuk, jadi di luar gate panjang.
        if initial.startswith("LD_"):
            continue
        tail = text[match.end():match.end() + 600]
        letter = re.search(
            r"PlayerTextDrawLetterSize\(playerid,\s*" + var + r"\[playerid\],\s*([^;]+)\);", tail
        )
        letter_args = _split_args(letter.group(1)) if letter else []
        size_x = _num(letter_args[0]) if len(letter_args) > 0 else 0.2
        size_y = _num(letter_args[1]) if len(letter_args) > 1 else 0.8
        color = re.search(r"PlayerTextDrawColor\(playerid,\s*" + var + r"\[playerid\],\s*(\w+)\)", tail)
        font = re.search(r"PlayerTextDrawFont\(playerid,\s*" + var + r"\[playerid\],\s*(\d+)\)", tail)
        draws.append({
            "var": var,
            "x": _num(x_arg),
            "y": _num(y_arg),
            "initial": initial,
            "size_x": size_x,
            "size_y": size_y,
            "color": color.group(1) if color else "UI_TEXT_PRIMARY",
            "font": int(font.group(1)) if font else 1,
        })
    return draws


def parse_fit_table(text: str) -> dict[str, tuple[float, float, int]]:
    """Ambil gHudFitRows: { "Nama", x, sizeX, worstLen }."""
    table: dict[str, tuple[float, float, int]] = {}
    body = re.search(r"gHudFitRows\[\]\[E_HUD_FIT\]\s*=\s*\{(.*?)\};", text, re.S)
    if not body:
        return table
    for name, x, size_x, worst in re.findall(
        r"\{\s*\"(\w+)\"\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*(\d+)\s*\}", body.group(1)
    ):
        table[name] = (float(x), float(size_x), int(worst))
    return table


def build_html(draws: list[dict], colors: dict[str, str]) -> str:
    rows = []
    for draw in draws:
        label = WORST_TEXT.get(draw["var"], draw["initial"])
        advance = draw["size_x"] * CHAR_ADVANCE_RATIO
        height = draw["size_y"] * LINE_HEIGHT_RATIO
        font_size = height * 1.30
        weight = 700 if draw["font"] == 2 else 500
        css_color = colors.get(draw["color"], "#f8fafc")
        width = len(label) * advance
        rows.append(
            f'<div class="td" style="left:{draw["x"]}px;top:{draw["y"]}px;'
            f'letter-spacing:{advance - font_size * 0.60:.2f}px;'
            f'font-size:{font_size:.1f}px;font-weight:{weight};'
            f"font-family:'DejaVu Sans Mono','Courier New',monospace;color:{css_color};\">"
            f'{label}'
            f'<span class="info">{draw["var"]} x={draw["x"]:.0f} y={draw["y"]:.0f} w≈{width:.0f}px</span></div>'
        )
    return f"""<!doctype html>
<html lang="id"><head><meta charset="utf-8"><title>HUD preview 640x480</title>
<style>
  html,body{{margin:0;padding:0;background:#0b0f14;}}
  #screen{{position:relative;width:640px;height:480px;background:#101720;overflow:hidden;}}
  .td{{position:absolute;white-space:pre;line-height:1.0;text-shadow:1px 1px 0 #000;}}
  .info{{position:absolute;left:-9999px;}}
</style></head>
<body><div id="screen">{''.join(rows)}</div></body></html>
"""


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_suffix(".html")
    hud_text = HUD.read_text(encoding="utf-8")
    colors = parse_colors(CONST.read_text(encoding="utf-8"))
    parse_defines(hud_text)
    draws = parse_textdraws(hud_text)
    table = parse_fit_table(hud_text)

    if not draws:
        print("GAGAL: tidak ada CreatePlayerTextDraw yang terbaca dari hud.inc")
        return 1
    if len(table) != len(draws):
        print(f"GAGAL: gHudFitRows berisi {len(table)} baris, HUD_Create punya {len(draws)}")
        return 1

    failures = 0
    print(f"{len(draws)} TextDraw, tabel gate {len(table)} baris\n")
    print(f"  {'baris':<12} {'x':>5} {'len':>4} {'lebar':>6} {'sisa':>6}  status")
    for draw in draws:
        name = draw["var"]
        label = WORST_TEXT.get(name, draw["initial"])
        width = measured_width(label, draw["size_x"])
        sisa = SCREEN_W - draw["x"] - width
        status = []

        # 1. drift terhadap tabel gate di Pawn
        fit = table.get(name)
        if fit is None:
            status.append("TIDAK ADA DI TABEL")
        elif abs(fit[0] - draw["x"]) > 0.01 or abs(fit[1] - draw["size_x"]) > 0.001:
            status.append(f"DRIFT (tabel x={fit[0]:.0f} sizeX={fit[1]:.2f})")
        elif fit[2] != len(label):
            status.append(f"DRIFT panjang (tabel {fit[2]} vs skrip {len(label)})")

        # 2. muat di layar
        if sisa < 0:
            status.append(f"MELUBER {-sisa:.0f}px")

        if status:
            failures += 1
        print(f"  {name:<12} {draw['x']:>5.0f} {len(label):>4} {width:>6.0f} {sisa:>6.0f}  "
              f"{'OK' if not status else ' | '.join(status)}")

    out.write_text(build_html(draws, colors), encoding="utf-8")
    print(f"\nHTML: {out}")

    if failures:
        print(f"GATE GAGAL: {failures} baris bermasalah")
        return 1
    print("GATE LULUS: semua baris muat dan tabel gate sinkron")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
