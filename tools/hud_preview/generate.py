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
    "HudVehicle": "BBM  100/100L pom 8000m TG",
    "HudBoss": "ABOMINATION 100%",
    "HudRaid": "RAID KAMP 14 ZOMBIE  3:00",
    "HudPromptText": "Y  Ekstraksi sampel air",
}


def parse_colors(text: str) -> dict[str, str]:
    colors: dict[str, str] = {}
    for name, value in re.findall(r"#define\s+(UI_\w+)\s+0x([0-9A-Fa-f]{8})", text):
        colors[name] = "#" + value[:6].lower()
    return colors


def parse_textdraws(text: str) -> list[dict]:
    draws: list[dict] = []
    create = re.compile(
        r"(Hud\w+)\[playerid\]\s*=\s*CreatePlayerTextDraw\(playerid,\s*"
        r"([-\d.]+),\s*([-\d.]+),\s*\"([^\"]*)\"\);",
        re.S,
    )
    for match in create.finditer(text):
        var, x, y, initial = match.group(1), float(match.group(2)), float(match.group(3)), match.group(4)
        # Sprite (LD_SPAC/LD_BEAT) dan TD berbasis array (kartu, bar) bukan teks:
        # tidak punya string terburuk, jadi di luar gate panjang.
        if initial.startswith("LD_"):
            continue
        tail = text[match.end():match.end() + 600]
        letter = re.search(
            r"PlayerTextDrawLetterSize\(playerid,\s*" + var + r"\[playerid\],\s*([-\d.]+),\s*([-\d.]+)\)", tail
        )
        color = re.search(r"PlayerTextDrawColor\(playerid,\s*" + var + r"\[playerid\],\s*(\w+)\)", tail)
        font = re.search(r"PlayerTextDrawFont\(playerid,\s*" + var + r"\[playerid\],\s*(\d+)\)", tail)
        draws.append({
            "var": var,
            "x": x,
            "y": y,
            "initial": initial,
            "size_x": float(letter.group(1)) if letter else 0.2,
            "size_y": float(letter.group(2)) if letter else 0.8,
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
        width = len(label) * draw["size_x"] * CHAR_ADVANCE_RATIO
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
