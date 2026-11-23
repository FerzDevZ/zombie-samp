#!/usr/bin/env python3
"""Pratinjau panel bangun (build_edit.inc) sebagai HTML 640x480.

Bukan emulator SA-MP: yang dirender hanya GEOMETRI (posisi, ukuran, teks
terburuk). Gunanya menangkap tumpang-tindih, tombol keluar kartu, dan teks
terpotong tanpa harus masuk game.

Angka diambil dari `gamemodes/modules/world/build_edit.inc`, bukan diketik
ulang: kalau panel digeser di kode, skrip ini gagal (exit 1) alih-alih
menampilkan gambar yang bohong.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD_EDIT = ROOT / "gamemodes/modules/world/build_edit.inc"
CONSTANTS = ROOT / "gamemodes/modules/config/constants.inc"

SCREEN_W, SCREEN_H = 640, 480

# Sama dengan HUD_CHAR_RATIO / LINE_HEIGHT_RATIO di hud.inc & generate.py.
CHAR_ADVANCE_RATIO = 35.3
LINE_HEIGHT_RATIO = 10.5

# Nilai contoh = nilai TERBURUK yang bisa muncul di panel.
ROWS = [
    "SAMPING +6.00 m",
    "MAJU    6.00 m",
    "NAIK    +3.00 m",
    "ARAH    359 deg",
    "JARAK   6.00 m",
    "LANGKAH 1.00 m / 45 deg",
]
SUB = "GENERATOR | TIER 3 | 12/12"
HINT = "ALT maju | C putar | Y pasang"

BUTTONS = [
    # (index, x_offset_from_panel, y_offset_from_panel, w, h, label, color)
    (0, 186.0, 40.0, 40.0, 12.0, "-", "elev"),
    (1, 228.0, 40.0, 40.0, 12.0, "+", "elev"),
    (2, 186.0, 56.0, 40.0, 12.0, "-", "elev"),
    (3, 228.0, 56.0, 40.0, 12.0, "+", "elev"),
    (4, 186.0, 72.0, 40.0, 12.0, "-", "elev"),
    (5, 228.0, 72.0, 40.0, 12.0, "+", "elev"),
    (6, 186.0, 88.0, 40.0, 12.0, "<", "elev"),
    (7, 228.0, 88.0, 40.0, 12.0, ">", "elev"),
    (8, 186.0, 104.0, 40.0, 12.0, "-", "elev"),
    (9, 228.0, 104.0, 40.0, 12.0, "+", "elev"),
    (10, 186.0, 120.0, 82.0, 12.0, "LANGKAH", "elev"),
    (11, 12.0, 142.0, 130.0, 20.0, "PASANG", "primary"),
    (12, 152.0, 142.0, 130.0, 20.0, "BATAL", "danger"),
    (13, 248.0, 6.0, 34.0, 13.0, "X", "elev"),
]


def read_defines(text: str) -> dict[str, float]:
    found: dict[str, float] = {}
    for name, value in re.findall(r"#define\s+(BUILD_EDIT_PANEL_\w+)\s+([-\d.]+)", text):
        found[name] = float(value)
    return found


def read_colors(text: str) -> dict[str, str]:
    colors: dict[str, str] = {}
    for name, value in re.findall(r"#define\s+(UI_\w+)\s+0x([0-9A-Fa-f]{8})", text):
        alpha = int(value[6:8], 16) / 255.0
        colors[name] = f"rgba({int(value[0:2], 16)},{int(value[2:4], 16)},{int(value[4:6], 16)},{alpha:.3f})"
    return colors


def require_literal(text: str, literal: str, why: str) -> None:
    if literal not in text:
        print(f"GAGAL: '{literal}' tidak ada di build_edit.inc ({why})", file=sys.stderr)
        sys.exit(1)


def build_html(defines: dict[str, float], colors: dict[str, str]) -> str:
    px = defines["BUILD_EDIT_PANEL_X"]
    py = defines["BUILD_EDIT_PANEL_Y"]
    pw = defines["BUILD_EDIT_PANEL_W"]
    ph = defines["BUILD_EDIT_PANEL_H"]

    def font(size_y: float) -> float:
        return round(size_y * LINE_HEIGHT_RATIO * 1.30, 1)

    parts: list[str] = []
    parts.append(
        f'<div class="card" style="left:{px}px;top:{py}px;width:{pw}px;height:{ph}px;'
        f'background:{colors["UI_BG_GLASS_DARK"]};"></div>'
    )
    parts.append(
        f'<div class="sprite" style="left:{px + 2}px;top:{py}px;width:{pw - 4}px;height:2px;'
        f'background:{colors["UI_ACCENT_DANGER"]};"></div>'
    )
    parts.append(
        f'<div class="text" style="left:{px + 12}px;top:{py + 5}px;font-size:{font(0.9)}px;'
        f'font-weight:800;letter-spacing:1px;color:{colors["UI_TEXT_PRIMARY"]};">PEMBANGUNAN</div>'
    )
    parts.append(
        f'<div class="text" style="left:{px + 12}px;top:{py + 21}px;font-size:{font(0.8)}px;'
        f'color:{colors["UI_TEXT_MUTED"]};">{SUB}</div>'
    )
    parts.append(
        f'<div class="sprite" style="left:{px + 12}px;top:{py + 34}px;width:{pw - 24}px;height:1px;'
        f'background:{colors["UI_BG_ELEVATED"]};"></div>'
    )

    for index, row in enumerate(ROWS):
        top = py + 40 + index * 16
        parts.append(
            f'<div class="text mono" style="left:{px + 12}px;top:{top}px;font-size:{font(0.85)}px;'
            f'color:{colors["UI_TEXT_SECONDARY"]};">{row}</div>'
        )

    parts.append(
        f'<div class="text" style="left:{px + 12}px;top:{py + 168}px;font-size:{font(0.75)}px;'
        f'color:{colors["UI_TEXT_MUTED"]};">{HINT}</div>'
    )

    for index, offset_x, offset_y, width, height, label, tone in BUTTONS:
        background = {
            "elev": colors["UI_BG_ELEVATED"],
            "primary": colors["UI_ACCENT_PRIMARY"],
            "danger": "rgba(127,29,29,0.949)",
        }[tone]
        left = px + offset_x
        top = py + offset_y
        parts.append(
            f'<div class="btn" style="left:{left}px;top:{top}px;width:{width}px;height:{height}px;'
            f'background:{background};" title="tombol {index}"></div>'
        )
        parts.append(
            f'<div class="btnlabel" style="left:{left}px;top:{top + height / 2 - 4.6}px;'
            f'width:{width}px;font-size:{font(0.8)}px;">{label}</div>'
        )

    return f"""<!doctype html>
<html lang="id"><head><meta charset="utf-8"><title>Panel bangun 640x480</title>
<style>
  html,body{{margin:0;padding:0;background:#0b0f14;}}
  #screen{{position:relative;width:{SCREEN_W}px;height:{SCREEN_H}px;
    background:linear-gradient(160deg,#141b24 0%,#0a0e14 60%,#1a1210 100%);overflow:hidden;}}
  .card{{position:absolute;border-radius:3px;}}
  .sprite{{position:absolute;}}
  .text{{position:absolute;font-family:'DejaVu Sans Mono','Courier New',monospace;line-height:1.0;
    white-space:pre;}}
  .mono{{letter-spacing:0;}}
  .btn{{position:absolute;border-radius:2px;box-shadow:0 0 0 1px rgba(255,255,255,0.06) inset;}}
  .btnlabel{{position:absolute;font-family:'DejaVu Sans Mono','Courier New',monospace;
    text-align:center;color:#f8fafc;line-height:1.0;}}
  #legend{{position:absolute;left:12px;top:12px;font:11px/1.5 'DejaVu Sans Mono',monospace;color:#64748b;}}
</style></head>
<body><div id="screen">
<div id="legend">pratinjau statis panel bangun<br>640x480 &middot; nilai terburuk</div>
{''.join(parts)}
</div></body></html>
"""


def main() -> int:
    code = BUILD_EDIT.read_text(encoding="utf-8")
    defines = read_defines(code)
    missing = {"BUILD_EDIT_PANEL_X", "BUILD_EDIT_PANEL_Y", "BUILD_EDIT_PANEL_W", "BUILD_EDIT_PANEL_H"} - defines.keys()
    if missing:
        print(f"GAGAL: define panel hilang: {sorted(missing)}", file=sys.stderr)
        return 1

    # Layout kini hidup di BuildEdit_ComputeLayout sebagai nilai struct per gaya;
    # kalau angka ini berubah di kode, skrip ini harus ikut diubah.
    require_literal(code, "gBeLayout[playerid][blColL] = 186.0;", "kolom tombol kiri PC")
    require_literal(code, "gBeLayout[playerid][blColR] = 228.0;", "kolom tombol kanan PC")
    require_literal(code, "gBeLayout[playerid][blRowStep] = 16.0;", "jarak baris tombol PC")
    require_literal(code, "gBeLayout[playerid][blPasangY] = 142.0;", "baris PASANG/BATAL PC")
    require_literal(code, "gBeLayout[playerid][blW] = 206.0;", "lebar panel mobile")
    require_literal(code, "gBeLayout[playerid][blH] = 224.0;", "tinggi panel mobile")
    require_literal(code, "gBeLayout[playerid][blBtnH] = 16.0;", "tinggi tombol mobile (sasaran sentuh)")
    require_literal(code, "gBeLayout[playerid][blRowStep] = 22.0;", "jarak baris tombol mobile")

    colors = read_colors(CONSTANTS.read_text(encoding="utf-8"))
    html = build_html(defines, colors)

    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_suffix(".html")
    out.write_text(html, encoding="utf-8")

    right = defines["BUILD_EDIT_PANEL_X"] + defines["BUILD_EDIT_PANEL_W"]
    bottom = defines["BUILD_EDIT_PANEL_Y"] + defines["BUILD_EDIT_PANEL_H"]
    print(f"panel: x={defines['BUILD_EDIT_PANEL_X']:.0f} y={defines['BUILD_EDIT_PANEL_Y']:.0f} "
          f"kanan={right:.0f}/{SCREEN_W} bawah={bottom:.0f}/{SCREEN_H} -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
