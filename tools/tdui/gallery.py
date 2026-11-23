#!/usr/bin/env python3
"""Galeri UI LAST STAND: render tata letak TextDraw & dialog ke PNG.

Sumber angka selalu file .inc yang dipakai server (hud.inc, build_edit.inc,
config/structures.inc, config/items.inc, config/recipes.inc). Kalau kode UI
berubah dan skrip ini tidak bisa memetakan ulang, build galeri GAGAL — supaya
folder docs/tdui tidak pernah berisi gambar yang tidak lagi mewakili kode.

Yang dirender adalah GEOMETRI + warna + teks terburuk, bukan screenshot client.
Dialog SA-MP adalah komponen native (bukan TextDraw), jadi digambar sebagai
mock dengan isi yang benar-benar dibangun server.

Pakai: python3 tools/tdui/gallery.py [--no-shoot]
"""

from __future__ import annotations

import importlib.util
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/tdui"
TMP = ROOT / "backups/tdui-html"

HUD_INC = ROOT / "gamemodes/modules/ui/hud.inc"
BUILD_EDIT = ROOT / "gamemodes/modules/world/build_edit.inc"
CONST = ROOT / "gamemodes/modules/config/constants.inc"
STRUCTURES = ROOT / "gamemodes/modules/config/structures.inc"
ITEMS = ROOT / "gamemodes/modules/config/items.inc"
RECIPES = ROOT / "gamemodes/modules/config/recipes.inc"

SCREEN_W, SCREEN_H = 640, 480
CHAR_ADVANCE_RATIO = 35.3
LINE_HEIGHT_RATIO = 10.5
SCALE = 2  # PNG 1280x960, geometri tetap 640x480

WATERMARK = "pratinjau tata letak 640x480 &middot; bukan screenshot client"


# --------------------------------------------------------------------- parsing

def load_hud_module():
    """Pakai parser HUD yang sudah ada supaya tidak ada regex kedua."""
    spec = importlib.util.spec_from_file_location(
        "hud_preview", ROOT / "tools/hud_preview/generate.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def parse_colors(text: str) -> dict[str, str]:
    colors: dict[str, str] = {}
    for name, value in re.findall(r"#define\s+(UI_\w+)\s+0x([0-9A-Fa-f]{8})", text):
        alpha = int(value[6:8], 16) / 255.0
        colors[name] = (f"rgba({int(value[0:2], 16)},{int(value[2:4], 16)},"
                        f"{int(value[4:6], 16)},{alpha:.3f})")
    return colors


def parse_defines(text: str, prefix: str) -> dict[str, float]:
    found: dict[str, float] = {}
    for name, value in re.findall(rf"#define\s+({prefix}\w+)\s+([-\d.]+)", text):
        found[name] = float(value)
    return found


def parse_items(text: str) -> dict[str, str]:
    """ITEM_SCRAP -> "Scrap", memakai urutan baris registry."""
    consts: dict[str, int] = {}
    for name, value in re.findall(r"const\s+(ITEM_\w+)\s*=\s*(\d+);", text):
        consts[name] = int(value)

    labels: dict[str, str] = {}
    registry = re.search(r"gItemRegistry\[ITEM_COUNT\]\[E_ITEM_DATA\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not registry:
        return {}
    rows = re.findall(r'\{\s*(-?\d+)\s*,\s*"([^"]*)"', registry.group(1))
    by_index = {i: label for i, (_, label) in enumerate(rows)}
    for name, value in consts.items():
        if value in by_index:
            labels[name] = by_index[value]
    return labels


def parse_structures(text: str, items: dict[str, str]) -> list[dict]:
    body = re.search(r"gStructures\[STRUCTURE_COUNT\]\[E_STRUCTURE\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not body:
        return []
    structures: list[dict] = []
    pattern = re.compile(
        r'\{\s*"([^"]+)"\s*,\s*(\w+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,\s*(\d+)\s*,'
        r"\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(true|false)\s*\}"
    )
    for match in pattern.finditer(body.group(1)):
        structures.append({
            "label": match.group(1),
            "mat_a": items.get(match.group(2), match.group(2).replace("ITEM_", "").title()),
            "amt_a": int(match.group(3)),
            "mat_b": items.get(match.group(4), match.group(4).replace("ITEM_", "").title()),
            "amt_b": int(match.group(5)),
            "health": int(match.group(6)),
            "tier": int(match.group(7)),
            "limit": int(match.group(8)),
        })
    return structures


def parse_recipes(text: str, items: dict[str, str]) -> list[dict]:
    body = re.search(r"gRecipes\[MAX_RECIPES\]\[E_RECIPE\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not body:
        return []
    recipes: list[dict] = []
    pattern = re.compile(
        r'\{\s*(\w+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,\s*"([^"]+)"\s*\}'
    )
    for match in pattern.finditer(body.group(1)):
        first = items.get(match.group(3), match.group(3).replace("ITEM_", "").title())
        amount_a = int(match.group(4))
        ingredient = f"{first} x{amount_a}"
        if match.group(5) not in ("ITEM_NONE_ID", "ITEM_NONE"):
            second = items.get(match.group(5), match.group(5).replace("ITEM_", "").title())
            ingredient += f" + {second} x{match.group(6)}"
        recipes.append({
            "name": match.group(8),
            "ingredient": ingredient,
            "blueprint": match.group(7) not in ("ITEM_NONE_ID", "ITEM_NONE"),
        })
    return recipes


def require_literal(text: str, literal: str, why: str) -> None:
    if literal not in text:
        print(f"GAGAL: literal '{literal}' tidak ada ({why}) — galeri tidak bisa "
              f"dijamin mewakili kode, perbarui tools/tdui/gallery.py", file=sys.stderr)
        raise SystemExit(1)


# --------------------------------------------------------------------- render

def page(title: str, body: str, backdrop: str, note: str = "") -> str:
    note_html = f'<div class="note">{note}</div>' if note else ""
    return f"""<!doctype html>
<html lang="id"><head><meta charset="utf-8"><title>{title}</title>
<style>
  html,body{{margin:0;padding:0;background:#05080c;}}
  #screen{{position:relative;width:{SCREEN_W}px;height:{SCREEN_H}px;overflow:hidden;
    background:{backdrop};font-family:'DejaVu Sans Mono','Courier New',monospace;}}
  .td{{position:absolute;white-space:pre;line-height:1.0;text-shadow:1.5px 1.5px 0 rgba(0,0,0,.85);}}
  .bar{{position:absolute;}}
  .mark{{position:absolute;left:10px;bottom:8px;font-size:10px;color:rgba(148,163,184,.55);
    letter-spacing:.4px;}}
  .note{{position:absolute;left:10px;top:8px;font-size:10px;color:rgba(148,163,184,.6);}}
  .ghost{{position:absolute;border:2px dashed rgba(248,250,252,.75);
    background:rgba(56,189,248,.14);}}
  .ghostlabel{{position:absolute;font-size:10px;color:rgba(248,250,252,.7);}}
  .card{{position:absolute;border-radius:3px;}}
  .btn{{position:absolute;border-radius:2px;box-shadow:0 0 0 1px rgba(255,255,255,.07) inset;}}
  .btnlabel{{position:absolute;text-align:center;color:#f8fafc;line-height:1.0;}}
  .dlg{{position:absolute;border:1px solid rgba(148,163,184,.35);border-radius:2px;
    background:rgba(6,10,16,.94);}}
  .dlghead{{position:absolute;background:rgba(30,41,59,.98);color:#e2e8f0;
    font-weight:700;text-align:center;letter-spacing:.5px;}}
  .dlgrow{{position:absolute;color:#cbd5e1;white-space:pre;}}
  .dlgbtn{{position:absolute;background:rgba(30,41,59,.98);color:#e2e8f0;text-align:center;
    border:1px solid rgba(148,163,184,.35);border-radius:2px;}}
  .legend{{position:absolute;right:12px;bottom:10px;font-size:10px;line-height:1.5;
    color:rgba(148,163,184,.75);text-align:right;}}
  .legend b{{color:#f8fafc;font-weight:600;}}
</style></head>
<body><div id="screen">
{body}
<div class="mark">{WATERMARK}</div>
{note_html}
</div></body></html>
"""


def td(x: float, y: float, text: str, size_x: float, size_y: float, color: str,
       weight: int = 500, mono: bool = True) -> str:
    advance = size_x * CHAR_ADVANCE_RATIO
    height = size_y * LINE_HEIGHT_RATIO
    font_size = height * 1.30
    family = "'DejaVu Sans Mono','Courier New',monospace" if mono else "'DejaVu Sans',Arial,sans-serif"
    spacing = advance - font_size * 0.60
    escaped = (text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))
    return (f'<div class="td" style="left:{x}px;top:{y}px;font-size:{font_size:.1f}px;'
            f"letter-spacing:{spacing:.2f}px;font-weight:{weight};font-family:{family};"
            f'color:{color};">{escaped}</div>')


def td_center(text: str, size_x: float, size_y: float, color: str) -> str:
    """Teks center (align 2): x=320 adalah titik tengah pil prompt."""
    advance = size_x * CHAR_ADVANCE_RATIO
    height = size_y * LINE_HEIGHT_RATIO
    font_size = height * 1.30
    spacing = advance - font_size * 0.60
    escaped = (text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))
    return (f'<div class="td" style="left:320px;top:418px;transform:translateX(-50%);'
            f'font-size:{font_size:.1f}px;letter-spacing:{spacing:.2f}px;font-weight:500;'
            f"font-family:'DejaVu Sans Mono','Courier New',monospace;"
            f'color:{color};">{escaped}</div>')


def bar(x: float, y: float, w: float, h: float, color: str) -> str:
    return f'<div class="bar" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px;background:{color};"></div>'


def ghost_wall(x: float, y: float, w: float, h: float, label: str) -> str:
    """Ilustrasi ghost object: persegi putus-putus, bukan proyeksi 3D palsu."""
    return (f'<div class="ghost" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px;"></div>'
            f'<div class="ghostlabel" style="left:{x}px;top:{y + h + 4}px;">{label}</div>')


# ------------------------------------------------------------------------ HUD

# Geometri kartu + bar HARUS sama dengan hud.inc; guard require_literal di main()
# membuat galeri gagal bila kode HUD berubah tanpa memperbarui file ini.
HUD_CARD = (16.0, 288.0, 288.0, 100.0)
HUD_ACCENT = (18.0, 288.0, 284.0, 2.0)
# index: (x, y, w, h) — HP/ARM/INF lalu LAP/HAUS/PIP
HUD_BARS = [
    (52.0, 312.0, 104.0, 5.0),
    (52.0, 324.0, 104.0, 5.0),
    (52.0, 336.0, 104.0, 5.0),
    (48.0, 348.0, 40.0, 4.0),
    (134.0, 348.0, 40.0, 4.0),
    (220.0, 348.0, 40.0, 4.0),
]
HUD_PROMPT = (210.0, 414.0, 220.0, 18.0)
# Bar bersegmen: (x, y, w, h, segmen) — celah digambar sebagai strip latar.
HUD_SEG_GAP_W = 1.6
HUD_SEG_BARS = {0: 10, 1: 10, 2: 10, 6: 10, 7: 10}  # index bar -> jumlah segmen
HUD_VEH_BAR = (500.0, 338.0, 118.0, 5.0)
HUD_BOSS_BAR = (210.0, 46.0, 258.0, 6.0)

HUD_SCENARIOS: dict[str, dict] = {
    # name: {"text": {...}, "bars": [(index, ratio, warna)], "hide": [...]}
    "01-hud-pejalanan": {
        "text": {
            "HudBrand": "LAST_STAND",
            "HudZone": "Bandara Lama [AMAN]",
            "HudHpVal": "87/100",
            "HudArmVal": "45/100",
            "HudInfVal": "12%",
            "HudLapVal": "62",
            "HudHausVal": "48",
            "HudPipVal": "21",
            "HudProgress": "LVL 7  SP 240  LUKA 1  /skill",
            "HudLoadout": "SCRAP 34  SAMPEL 12",
            "HudPromptText": "Y  Stash Kamp",
        },
        "bars": [
            (0, 0.87, "UI_ACCENT_PRIMARY"), (1, 0.45, "UI_ACCENT_INFO"),
            (2, 0.12, "UI_ACCENT_PRIMARY"), (3, 0.62, "UI_ACCENT_PRIMARY"),
            (4, 0.48, "UI_ACCENT_PRIMARY"), (5, 0.79, "UI_ACCENT_PRIMARY"),
        ],
        "hide": ["HudVehicle", "HudBoss", "HudRaid"],
        "note": "pejalanan dekat stash - bar HP/ARM/INF + prompt interaksi [Y]",
    },
    "02-hud-kendaraan": {
        "text": {
            "HudBrand": "LAST_STAND",
            "HudZone": "Jalan Raya [AMAN]",
            "HudHpVal": "92/100",
            "HudArmVal": "60/100",
            "HudInfVal": "8%",
            "HudLapVal": "55",
            "HudHausVal": "41",
            "HudPipVal": "30",
            "HudProgress": "LVL 7  SP 240  LUKA 0  /skill",
            "HudLoadout": "SCRAP 34  SAMPEL 12",
            "HudVehicle": "BBM  62/100L 240km",
        },
        "veh_bar": (0.62, "UI_ACCENT_PRIMARY"),
        "bars": [
            (0, 0.92, "UI_ACCENT_PRIMARY"), (1, 0.60, "UI_ACCENT_INFO"),
            (2, 0.08, "UI_ACCENT_PRIMARY"), (3, 0.55, "UI_ACCENT_PRIMARY"),
            (4, 0.41, "UI_ACCENT_PRIMARY"), (5, 0.70, "UI_ACCENT_PRIMARY"),
        ],
        "hide": ["HudBoss", "HudRaid", "HudPromptText"],
        "note": "di dalam kendaraan - bar BBM (pengemudi & penumpang), kolom kanan sejajar",
    },
    "03-hud-generator": {
        "text": {
            "HudBrand": "LAST_STAND",
            "HudZone": "Base Kamp [AMAN]",
            "HudHpVal": "74/100",
            "HudArmVal": "20/100",
            "HudInfVal": "21%",
            "HudLapVal": "40",
            "HudHausVal": "33",
            "HudPipVal": "18",
            "HudProgress": "LVL 7  SP 240  LUKA 1  /skill",
            "HudLoadout": "SCRAP 51  SAMPEL 12",
            "HudVehicle": "GENSET  12/20L",
        },
        "veh_bar": (0.60, "UI_ACCENT_PRIMARY"),
        "bars": [
            (0, 0.74, "UI_ACCENT_PRIMARY"), (1, 0.20, "UI_ACCENT_INFO"),
            (2, 0.21, "UI_ACCENT_PRIMARY"), (3, 0.40, "UI_ACCENT_WARN"),
            (4, 0.33, "UI_ACCENT_WARN"), (5, 0.82, "UI_ACCENT_PRIMARY"),
        ],
        "hide": ["HudBoss", "HudRaid", "HudPromptText"],
        "note": "di dekat genset base sendiri (<= 12 m) - baris GENSET menggantikan BBM",
    },
    "04-hud-kritis": {
        "text": {
            "HudBrand": "LAST_STAND",
            "HudZone": "Kota Los Santos [WASPADA]",
            "HudHpVal": "14/100",
            "HudArmVal": "0/100",
            "HudInfVal": "78%",
            "HudLapVal": "3",
            "HudHausVal": "8",
            "HudPipVal": "74",
            "HudProgress": "LVL 7  SP 240  LUKA 3  /skill",
            "HudLoadout": "SCRAP 4  SAMPEL 0",
            "HudVehicle": "BBM  18/100L pom 240m BD",
            "HudBoss": "ABOMINATION 100%",
            "HudRaid": "RAID KAMP 14 ZOMBIE  3:00",
        },
        "veh_bar": (0.0, "UI_ACCENT_DANGER"),  # <=25%: bar disembunyikan, teks pom full width
        "boss_bar": 1.0,
        "bars": [
            (0, 0.14, "UI_ACCENT_DANGER"), (1, 0.0, "UI_ACCENT_INFO"),
            (2, 0.78, "UI_ACCENT_DANGER"), (3, 0.03, "UI_ACCENT_DANGER"),
            (4, 0.08, "UI_ACCENT_DANGER"), (5, 0.26, "UI_ACCENT_DANGER"),
        ],
        "hide": ["HudPromptText"],
        "note": ("semua peringatan aktif - bar merah = kritis; warna bar dihitung "
                 "runtime, gambar memakai warna state yang sama"),
    },
    "11-hud-mobile": {
        "gaya": 2,
        "text": {
            "HudBrand": "LAST_STAND",
            "HudZone": "Bandara Lama [AMAN]",
            "HudHpVal": "87/100",
            "HudArmVal": "45/100",
            "HudInfVal": "12%",
            "HudLapVal": "62",
            "HudHausVal": "48",
            "HudPipVal": "21",
            "HudProgress": "LVL 7  SP 240  LUKA 1",
            "HudLoadout": "SCRAP 34  SAMPEL 12",
            "HudPromptText": "Y  Stash Kamp",
        },
        "bars": [
            (0, 0.87, "UI_ACCENT_PRIMARY"), (1, 0.45, "UI_ACCENT_INFO"),
            (2, 0.12, "UI_ACCENT_PRIMARY"), (3, 0.62, "UI_ACCENT_PRIMARY"),
            (4, 0.48, "UI_ACCENT_PRIMARY"), (5, 0.79, "UI_ACCENT_PRIMARY"),
        ],
        "hide": ["HudVehicle", "HudBoss", "HudRaid"],
        "note": "gaya 2 MOBILE KOMPAK (skala 0.70, Android otomatis) - kartu naik ke y=202, keluar dari zona joystick & tombol sentuh",
    },
    "12-hud-minimal": {
        "gaya": 3,
        "text": {
            "HudBrand": "LAST_STAND",
            "HudZone": "Bandara Lama [AMAN]",
            "HudHpVal": "87/100",
            "HudArmVal": "45/100",
            "HudInfVal": "12%",
            "HudLapVal": "62",
            "HudHausVal": "48",
            "HudPipVal": "21",
            "HudProgress": "LVL 7  SP 240  LUKA 1",
            "HudLoadout": "SCRAP 34  SAMPEL 12",
        },
        "bars": [
            (0, 0.87, "UI_ACCENT_PRIMARY"), (1, 0.45, "UI_ACCENT_INFO"),
            (2, 0.12, "UI_ACCENT_PRIMARY"), (3, 0.62, "UI_ACCENT_PRIMARY"),
            (4, 0.48, "UI_ACCENT_PRIMARY"), (5, 0.79, "UI_ACCENT_PRIMARY"),
        ],
        "hide": ["HudVehicle", "HudBoss", "HudRaid", "HudPromptText"],
        "note": "gaya 3 MINIMALIS (skala 0.85) - tanpa kartu/kotak, hanya bar + angka; /hudgaya 3",
    },
}


HUD_SCALE = {1: 1.0, 2: 0.70, 3: 0.85}  # cermin HUD_ScaleOf di hud.inc
HUD_CENTERED = {"HudBoss", "HudRaid", "HudPromptText"}  # pemakai HUD_XC


def _hx(x: float, s: float) -> float:
    return x * s


def _hxc(x: float, s: float) -> float:
    return 320.0 - (320.0 - x) * s


def render_hud(scene: dict, draws: list[dict], colors: dict[str, str],
               fit: dict[str, tuple[float, float, int]]) -> str:
    gaya = scene.get("gaya", 1)
    sc = HUD_SCALE[gaya]
    hidden = set(scene["hide"])
    parts: list[str] = []

    # kartu kaca rounded + aksen tipis (pola 2 strip + 4 chit di hud.inc)
    if gaya != 3:  # gaya minimalis tanpa kartu (HUD_GayaHasCard)
        cx, cy, cw, ch = HUD_CARD
        accent = colors["UI_ACCENT_PRIMARY"]
        parts.append(
            f'<div style="position:absolute;left:{_hx(cx, sc):.1f}px;top:{_hx(cy, sc):.1f}px;'
            f'width:{_hx(cw, sc):.1f}px;height:{_hx(ch, sc):.1f}px;'
            f'background:{colors["UI_BG_GLASS_DARK"]};border:1px solid rgba(148,163,184,0.18);'
            f'border-radius:4px;"></div>')
        ax, ay, aw, ah = HUD_ACCENT
        parts.append(
            f'<div style="position:absolute;left:{_hx(ax, sc):.1f}px;top:{_hx(ay, sc):.1f}px;'
            f'width:{_hx(aw, sc):.1f}px;height:{_hx(ah, sc):.1f}px;'
            f'background:{accent};border-radius:1px;"></div>')

    # bar: track (UI_BG_ELEVATED) + fill dinamis + celah segmen
    for index, (bx, by, bw, bh) in enumerate(HUD_BARS):
        parts.append(
            f'<div style="position:absolute;left:{_hx(bx, sc):.1f}px;top:{_hx(by, sc):.1f}px;'
            f'width:{_hx(bw, sc):.1f}px;height:{_hx(bh, sc):.1f}px;'
            f'background:{colors["UI_BG_ELEVATED"]};border-radius:1px;"></div>')
    for index, ratio, color_name in scene["bars"]:
        bx, by, bw, bh = HUD_BARS[index]
        bw, bh = _hx(bw, sc), _hx(bh, sc)
        fill_w = max(0.0, min(1.0, ratio)) * bw
        parts.append(
            f'<div style="position:absolute;left:{_hx(bx, sc):.1f}px;top:{_hx(by, sc):.1f}px;'
            f'width:{fill_w:.1f}px;height:{bh:.1f}px;'
            f'background:{colors[color_name]};border-radius:1px;"></div>')
        segments = HUD_SEG_BARS.get(index, 0)
        for cut in range(1, segments):
            gap_x = _hx(bx, sc) + cut * (bw / segments) - HUD_SEG_GAP_W * sc * 0.5
            parts.append(
                f'<div style="position:absolute;left:{gap_x:.1f}px;top:{_hx(by, sc):.1f}px;'
                f'width:{HUD_SEG_GAP_W * sc:.1f}px;height:{bh:.1f}px;'
                f'background:{colors["UI_BG_GLASS_DARK"]};"></div>')

    # bar BBM/GENSET di dalam kotak kanan: track+fill+celah hanya saat rasio > 0
    # (sama dengan Pawn: bar hilang utuh saat BBM <= 25% / genset mati)
    vx, vy, vw, vh = HUD_VEH_BAR
    veh_ratio, veh_color = scene.get("veh_bar", (0.0, "UI_ACCENT_PRIMARY"))
    if "HudVehicle" not in hidden and veh_ratio > 0.0:
        vw, vh = _hx(vw, sc), _hx(vh, sc)
        parts.append(
            f'<div style="position:absolute;left:{_hx(vx, sc):.1f}px;top:{_hx(vy, sc):.1f}px;'
            f'width:{vw:.1f}px;height:{vh:.1f}px;'
            f'background:{colors["UI_BG_ELEVATED"]};border-radius:1px;"></div>')
        fill_w = max(0.0, min(1.0, veh_ratio)) * vw
        if fill_w > 0:
            parts.append(
                f'<div style="position:absolute;left:{_hx(vx, sc):.1f}px;top:{_hx(vy, sc):.1f}px;'
                f'width:{fill_w:.1f}px;height:{vh:.1f}px;'
                f'background:{colors[veh_color]};border-radius:1px;"></div>')
        for cut in range(1, 10):
            gap_x = _hx(vx, sc) + cut * (vw / 10) - HUD_SEG_GAP_W * sc * 0.5
            parts.append(
                f'<div style="position:absolute;left:{gap_x:.1f}px;top:{_hx(vy, sc):.1f}px;'
                f'width:{HUD_SEG_GAP_W * sc:.1f}px;height:{vh:.1f}px;background:rgba(11,15,20,0.71);"></div>')

    # bar boss di bawah teks (tampil bila HudBoss tidak disembunyikan)
    ox, oy, ow, oh = HUD_BOSS_BAR
    if "HudBoss" not in hidden:
        ow, oh = _hx(ow, sc), _hx(oh, sc)
        parts.append(
            f'<div style="position:absolute;left:{_hxc(ox, sc):.1f}px;top:{_hx(oy, sc):.1f}px;'
            f'width:{ow:.1f}px;height:{oh:.1f}px;'
            f'background:{colors["UI_BG_ELEVATED"]};border-radius:2px;"></div>')
        boss_ratio = scene.get("boss_bar", 1.0)
        fill_w = max(0.0, min(1.0, boss_ratio)) * ow
        if fill_w > 0:
            parts.append(
                f'<div style="position:absolute;left:{_hxc(ox, sc):.1f}px;top:{_hx(oy, sc):.1f}px;'
                f'width:{fill_w:.1f}px;height:{oh:.1f}px;'
                f'background:{colors["UI_ACCENT_DANGER"]};border-radius:2px;"></div>')
        for cut in range(1, 10):
            gap_x = _hxc(ox, sc) + cut * (ow / 10) - HUD_SEG_GAP_W * sc * 0.5
            parts.append(
                f'<div style="position:absolute;left:{gap_x:.1f}px;top:{_hx(oy, sc):.1f}px;'
                f'width:{HUD_SEG_GAP_W * sc:.1f}px;height:{oh:.1f}px;background:rgba(11,15,20,0.6);"></div>')

    # pil prompt interaksi
    if "HudPromptText" not in hidden and gaya != 3:
        px, py, pw, ph = HUD_PROMPT
        parts.append(
            f'<div style="position:absolute;left:{_hxc(px, sc):.1f}px;top:{_hx(py, sc):.1f}px;'
            f'width:{_hx(pw, sc):.1f}px;height:{_hx(ph, sc):.1f}px;'
            f'background:rgba(11,15,20,0.9);border:1px solid rgba(148,163,184,0.25);'
            f'border-radius:9px;"></div>')

    for draw in draws:
        name = draw["var"]
        if name in hidden:
            continue
        text = scene["text"].get(name, draw["initial"])
        size_x = (fit[name][1] if name in fit else draw["size_x"]) * sc
        dx = _hxc(draw["x"], sc) if name in HUD_CENTERED else _hx(draw["x"], sc)
        dy = _hx(draw["y"], sc)
        if name == "HudPromptText":
            # align 2 = center: x adalah titik tengah, bukan tepi kiri
            parts.append(td_center(text, size_x, draw["size_y"], colors.get(draw["color"], "#f8fafc")))
            continue
        parts.append(td(dx, dy, text, size_x, draw["size_y"],
                        colors.get(draw["color"], "#f8fafc"),
                        weight=700 if draw["font"] == 2 else 500,
                        mono=draw["font"] != 2))
    return "".join(parts)


# ---------------------------------------------------------------- panel bangun

def be_layout(mobile: bool) -> dict:
    """Cermin BuildEdit_ComputeLayout di build_edit.inc - angka HARUS sama."""
    if mobile:
        return {"x": 640.0 - (640.0 - 334.0) * 0.70, "y": 28.0, "w": 206.0, "h": 224.0,
                "btn_w": 38.0, "btn_h": 16.0, "row_step": 22.0, "info_step": 13.0,
                "col_l": 116.0, "col_r": 160.0, "pasang_x": 10.0, "pasang_w": 90.0,
                "pasang_y": 170.0, "pasang_h": 22.0, "hint_y": 200.0, "font_row": 0.13}
    return {"x": 334.0, "y": 40.0, "w": 294.0, "h": 210.0,
            "btn_w": 40.0, "btn_h": 12.0, "row_step": 16.0, "info_step": 16.0,
            "col_l": 186.0, "col_r": 228.0, "pasang_x": 12.0, "pasang_w": 130.0,
            "pasang_y": 142.0, "pasang_h": 20.0, "hint_y": 168.0, "font_row": 0.19}


def render_build_panel(mode: str, colors: dict[str, str], defines: dict[str, float],
                       mobile: bool = False) -> str:
    lay = be_layout(mobile)
    px, py, pw, ph = lay["x"], lay["y"], lay["w"], lay["h"]
    row_step, info_step = lay["row_step"], lay["info_step"]

    if mode == "place":
        rows = ([
            "SAMPING +0.0 m",
            "MAJU 2.5 m",
            "NAIK +0.0 m",
            "ARAH 90 deg",
            "JARAK 2.5 m",
            "LANGKAH 0.5/15",
        ] if mobile else [
            "SAMPING +0.00 m",
            "MAJU    2.50 m",
            "NAIK    +0.00 m",
            "ARAH    90 deg",
            "JARAK   2.50 m",
            "LANGKAH 0.50 m / 15 deg",
        ])
        sub = "WALL | TIER 1 | 9/12"
        confirm = colors["UI_ACCENT_PRIMARY"]
        hint = "ALT maju | C putar | Y pasang"
        ghost = ghost_wall(258.0, 250.0, 128.0, 74.0,
                           "ghost object 1225 - posisi PASANG (ilustrasi)")
    else:
        rows = ([
            "X 1676.50",
            "Y -2325.10",
            "Z 13.90",
            "ARAH 45 deg",
            "JARAK 3.2 m",
            "LANGKAH 0.3/5",
        ] if mobile else [
            "X       1676.50",
            "Y       -2325.10",
            "Z       13.90",
            "ARAH    45 deg",
            "JARAK   3.20 m",
            "LANGKAH 0.25 m / 5 deg",
        ])
        sub = "WALL | MODE GESER | #12"
        confirm = colors["UI_ACCENT_WARN"]
        hint = "ESC keluar | /panel lagi"
        ghost = ghost_wall(258.0, 250.0, 128.0, 74.0,
                           "ghost object 1225 - posisi lama (ilustrasi)")

    parts: list[str] = []
    parts.append(f'<div class="card" style="left:{px}px;top:{py}px;width:{pw}px;height:{ph}px;'
                 f'background:{colors["UI_BG_GLASS_DARK"]};"></div>')
    parts.append(bar(px + 2, py, pw - 4, 2, colors["UI_ACCENT_DANGER"]))
    parts.append(td(px + 12, py + 5, "PEMBANGUNAN", 0.22, 0.9,
                    colors["UI_TEXT_PRIMARY"], weight=800, mono=False))
    parts.append(td(px + 12, py + 21, sub, 0.16, 0.8, colors["UI_TEXT_MUTED"], weight=400, mono=False))
    parts.append(bar(px + 12, py + 34, pw - 24, 1, colors["UI_BG_ELEVATED"]))

    for index, row in enumerate(rows):
        parts.append(td(px + 12, py + 40 + index * info_step, row, lay["font_row"], 0.85,
                        colors["UI_TEXT_SECONDARY"]))

    parts.append(td(px + 12, py + lay["hint_y"], hint, 0.15, 0.75, colors["UI_TEXT_MUTED"], weight=400, mono=False))

    bw, bh = lay["btn_w"], lay["btn_h"]
    buttons = []
    for n in range(3):  # X / Y / Z
        ry = 40.0 + n * row_step
        buttons.append((lay["col_l"], ry, bw, "-", "elev"))
        buttons.append((lay["col_r"], ry, bw, "+", "elev"))
    ry = 40.0 + 3 * row_step
    buttons.append((lay["col_l"], ry, bw, "<", "elev"))
    buttons.append((lay["col_r"], ry, bw, ">", "elev"))
    ry = 40.0 + 4 * row_step
    buttons.append((lay["col_l"], ry, bw, "-", "elev"))
    buttons.append((lay["col_r"], ry, bw, "+", "elev"))
    ry = 40.0 + 5 * row_step
    buttons.append((lay["col_l"], ry, bw * 2 + 6.0, "LANGKAH", "elev"))
    buttons.append((lay["pasang_x"], lay["pasang_y"], lay["pasang_w"], "PASANG", "confirm"))
    buttons.append((pw - lay["pasang_w"] - lay["pasang_x"], lay["pasang_y"], lay["pasang_w"], "BATAL", "danger"))
    buttons.append((pw - 30.0, 6.0, 24.0, "X", "elev"))

    fills = {
        "elev": colors["UI_BG_ELEVATED"],
        "confirm": confirm,
        "danger": "rgba(127,29,29,0.949)",
    }
    for offset_x, offset_y, width, label, tone in buttons:
        height = lay["pasang_h"] if offset_y >= lay["pasang_y"] else (14.0 if offset_y < 40.0 else bh)
        left = px + offset_x
        top = py + offset_y
        parts.append(f'<div class="btn" style="left:{left:.1f}px;top:{top:.1f}px;width:{width:.1f}px;'
                     f'height:{height:.1f}px;background:{fills[tone]};"></div>')
        parts.append(f'<div class="btnlabel" style="left:{left:.1f}px;top:{top + height / 2 - 4.6:.1f}px;'
                     f'width:{width:.1f}px;font-size:{0.8 * LINE_HEIGHT_RATIO * 1.3:.1f}px;">{label}</div>')

    parts.append(ghost)
    return "".join(parts)


# --------------------------------------------------------------------- dialog

def render_dialog(title: str, rows: list[str], footer: tuple[str, str],
                  header: tuple[str, str] | None = None, note: str = "") -> str:
    """Mock dialog native SA-MP (bukan TextDraw): header bar + baris rata kiri."""
    line_h = 13.0
    width = 468.0
    body_rows = ([f"{header[0]}\t{header[1]}"] if header else []) + rows
    height = 34.0 + len(body_rows) * line_h + 30.0
    left = (SCREEN_W - width) / 2.0
    top = (SCREEN_H - height) / 2.0

    parts: list[str] = []
    parts.append(f'<div class="dlg" style="left:{left}px;top:{top}px;width:{width}px;height:{height}px;"></div>')
    parts.append(f'<div class="dlghead" style="left:{left}px;top:{top}px;width:{width}px;'
                 f'height:20px;line-height:20px;font-size:11px;">{title}</div>')

    for index, row in enumerate(body_rows):
        row_top = top + 28.0 + index * line_h
        is_header = header is not None and index == 0
        color = "#94a3b8" if is_header else "#cbd5e1"
        weight = 700 if is_header else 400
        parts.append(f'<div class="dlgrow" style="left:{left + 12}px;top:{row_top}px;'
                     f'font-size:10.5px;font-weight:{weight};color:{color};">'
                     f'{row.replace("&", "&amp;").replace("<", "&lt;")}</div>')

    button_y = top + height - 24.0
    button_w = 120.0
    parts.append(f'<div class="dlgbtn" style="left:{left + 14}px;top:{button_y}px;'
                 f'width:{button_w}px;height:17px;line-height:17px;font-size:10.5px;">{footer[0]}</div>')
    parts.append(f'<div class="dlgbtn" style="left:{left + width - 14 - button_w}px;top:{button_y}px;'
                 f'width:{button_w}px;height:17px;line-height:17px;font-size:10.5px;">{footer[1]}</div>')

    if note:
        parts.append(f'<div class="note">{note}</div>')
    return "".join(parts)


# ----------------------------------------------------------------------- main

BACKDROP_GAME = "linear-gradient(165deg,#16202b 0%,#0b1118 55%,#1b120f 100%)"
BACKDROP_HUD = "linear-gradient(165deg,#121a24 0%,#0a0f15 55%,#171310 100%)"
BACKDROP_DIALOG = "linear-gradient(165deg,#0d1520 0%,#080c11 60%,#14100e 100%)"


def build_dialog_rows(structures: list[dict], tier: int, used: dict[str, int]) -> list[str]:
    rows: list[str] = []
    for structure in structures:
        limit = structure["limit"]
        if structure["label"] == "Wall" and tier >= 2:
            limit += 4
        taken = used.get(structure["label"], 0)
        if tier < structure["tier"]:
            status = f"butuh tier {structure['tier']}"
        elif taken >= limit:
            status = "penuh"
        elif structure["label"] in ("Turret",):
            status = "bahan kurang"
        else:
            status = "siap"
        rows.append(
            f"{structure['label']} ({taken}/{limit}) - "
            f"{structure['mat_a']} {structure['amt_a']} + {structure['mat_b']} {structure['amt_b']} [{status}]"
        )
    return rows


def write_page(name: str, title: str, body: str, backdrop: str) -> Path:
    TMP.mkdir(parents=True, exist_ok=True)
    path = TMP / f"{name}.html"
    path.write_text(page(title, body, backdrop), encoding="utf-8")
    return path


def shoot(html: Path, png: Path) -> bool:
    chrome = shutil.which("google-chrome") or shutil.which("chromium") or shutil.which("chromium-browser")
    if not chrome:
        print(f"  ! chrome tidak ada, lewati {png.name}")
        return False
    png.parent.mkdir(parents=True, exist_ok=True)
    cmd = [chrome, "--headless", "--disable-gpu", "--hide-scrollbars", "--no-sandbox",
           f"--force-device-scale-factor={SCALE}",
           f"--window-size={SCREEN_W},{SCREEN_H}",
           f"--screenshot={png}", f"file://{html}"]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=90)
    if not png.exists():
        print(f"  ! screenshot gagal {png.name}: {result.stderr.strip().splitlines()[:1]}")
        return False
    return True


def main() -> int:
    shoot_pngs = "--no-shoot" not in sys.argv

    colors = parse_colors(CONST.read_text(encoding="utf-8"))
    hud_text = HUD_INC.read_text(encoding="utf-8")
    edit_text = BUILD_EDIT.read_text(encoding="utf-8")
    structures_text = STRUCTURES.read_text(encoding="utf-8")
    items_text = ITEMS.read_text(encoding="utf-8")
    recipes_text = RECIPES.read_text(encoding="utf-8")
    menu_text = (ROOT / "gamemodes/modules/ui/menu.inc").read_text(encoding="utf-8")
    panel_text = (ROOT / "gamemodes/modules/admin/panel.inc").read_text(encoding="utf-8")

    hud_module = load_hud_module()
    draws = hud_module.parse_textdraws(hud_text)
    fit = hud_module.parse_fit_table(hud_text)
    if not draws or len(fit) != len(draws):
        print(f"GAGAL: HUD terbaca {len(draws)} TextDraw, tabel gate {len(fit)} baris", file=sys.stderr)
        return 1

    defines = parse_defines(edit_text, "BUILD_EDIT_PANEL_")
    missing = {"BUILD_EDIT_PANEL_X", "BUILD_EDIT_PANEL_Y",
               "BUILD_EDIT_PANEL_W", "BUILD_EDIT_PANEL_H"} - defines.keys()
    if missing:
        print(f"GAGAL: define panel hilang: {sorted(missing)}", file=sys.stderr)
        return 1
    require_literal(edit_text, "gBeLayout[playerid][blColL] = 186.0;", "kolom tombol kiri PC")
    require_literal(edit_text, "gBeLayout[playerid][blColR] = 228.0;", "kolom tombol kanan PC")
    require_literal(edit_text, "gBeLayout[playerid][blPasangY] = 142.0;", "baris PASANG/BATAL PC")
    require_literal(edit_text, "gBeLayout[playerid][blW] = 206.0;", "lebar panel mobile")
    require_literal(edit_text, "gBeLayout[playerid][blBtnH] = 16.0;", "tinggi tombol mobile")

    # Geometri kartu/bar/prompt HUD harus masih persis di hud.inc — kalau angka
    # ini berubah, galeri (dan pemahaman kita tentang layar) basi.
    require_literal(hud_text, "HUD_CreateCard(playerid, HUD_X(playerid, 16.0), HUD_Y(playerid, 328.0),", "kartu HUD")
    require_literal(hud_text, "HUD_FX(playerid, 288.0), HUD_F(playerid, 100.0), HUD_F(playerid, 2.0),", "ukuran kartu HUD")
    require_literal(hud_text, "gHudBarW[HUD_BAR_COUNT] = {104.0, 104.0, 104.0, 40.0, 40.0, 40.0, 264.0, 258.0, 264.0, 264.0}", "lebar bar HUD v5")
    require_literal(hud_text, "HUD_CreateBar(playerid, HUD_BAR_HP, HUD_X(playerid, 52.0), HUD_Y(playerid, 352.0), gHudSkins[skIdx][skinTrack], gHudSkins[skIdx][skinCard]", "bar HP")
    require_literal(hud_text, "HUD_CreateBar(playerid, HUD_BAR_PIP, HUD_X(playerid, 220.0), HUD_Y(playerid, 388.0)", "bar PIP")
    require_literal(hud_text, "CreatePlayerTextDraw(playerid, HUD_XC(playerid, 210.0), HUD_Y(playerid, 448.0)", "pil prompt")
    require_literal(hud_text, "PlayerTextDrawTextSize(playerid, HudPromptBg[playerid], HUD_FX(playerid, 220.0), HUD_F(playerid, 18.0))", "ukuran pil prompt")

    require_literal(hud_text, "HUD_CreateBar(playerid, HUD_BAR_VEH, HUD_X(playerid, HUD_VEH_BAR_X),", "bar BBM")
    require_literal(hud_text, "HUD_CreateBar(playerid, HUD_BAR_GPS, HUD_X(playerid, HUD_VEHC_TXT_X),", "bar GPS")
    require_literal(hud_text, "HUD_CreateBar(playerid, HUD_BAR_BOSS, HUD_XC(playerid, 210.0), HUD_Y(playerid, 46.0)", "bar boss")
    require_literal(hud_text, "#define HUD_SEG_GAP_W    1.6", "lebar celah segmen")
    require_literal(hud_text, "stock Float:HUD_XC(playerid, Float:x)", "helper tengah layar")
    require_literal(CONST.read_text(encoding="utf-8"), "#define HUD_SCALE_MOBILE            0.70", "skala gaya mobile")
    require_literal(CONST.read_text(encoding="utf-8"), "#define HUD_SCALE_MINIMAL           0.85", "skala gaya minimal")
    require_literal(edit_text, "gBeLayout[playerid][blW] = 206.0;", "lebar panel bangun mobile")
    require_literal(edit_text, "gBeLayout[playerid][blH] = 224.0;", "tinggi panel bangun mobile")
    require_literal(edit_text, "gBeLayout[playerid][blBtnH] = 16.0;", "tinggi tombol bangun mobile")
    require_literal(edit_text, "Streamer_Update(playerid, STREAMER_TYPE_OBJECT);", "ghost gerak real-time")

    items = parse_items(items_text)
    structures = parse_structures(structures_text, items)
    recipes = parse_recipes(recipes_text, items)
    if not structures or not recipes or not items:
        print(f"GAGAL: data config tidak terbaca "
              f"(struktur={len(structures)} resep={len(recipes)} item={len(items)})", file=sys.stderr)
        return 1

    jobs: list[tuple[str, str, str, str]] = []  # (name, title, body, backdrop)

    for name, scene in HUD_SCENARIOS.items():
        body = render_hud(scene, draws, colors, fit)
        body += f'<div class="note">{scene["note"]}</div>'
        jobs.append((name, f"HUD {name}", body, BACKDROP_HUD))

    jobs.append(("05-bangun-pratinjau", "Panel bangun - mode PASANG",
                 render_build_panel("place", colors, defines)
                 + '<div class="note">mode PASANG - material dipotong saat tombol PASANG (hijau = cukup)</div>',
                 BACKDROP_GAME))
    jobs.append(("06-bangun-geser", "Panel bangun - mode GESER",
                 render_build_panel("move", colors, defines)
                 + '<div class="note">mode GESER - baris berubah jadi koordinat dunia X/Y/Z</div>',
                 BACKDROP_GAME))
    jobs.append(("13-bangun-mobile", "Panel bangun - MOBILE KOMPAK",
                 render_build_panel("place", colors, defines, mobile=True)
                 + '<div class="note">gaya 2 mobile: panel 206x224 ditempel kanan-atas, tombol 38x16 tetap bisa ditumbuk jari</div>',
                 BACKDROP_GAME))

    used = {"Wall": 9, "Gate": 1, "Crate": 2, "Bench": 1, "Bed": 0, "Turret": 0, "Generator": 0}
    build_rows = build_dialog_rows(structures, 2, used)
    built_total = sum(used.values())
    jobs.append(("07-dialog-bangun", "Dialog bangun base",
                 render_dialog(f"Bangun base - tier 2 | {built_total}/40 struktur",
                               build_rows + ["", "Dibongkar: /bongkar | Perbaiki: /fix | Naik tier: /upgrade"],
                               ("Bangun", "Tutup"),
                               note="dialog native SA-MP (DIALOG_STYLE_LIST), isi dibangun dari config/structures.inc"),
                 BACKDROP_DIALOG))

    require_literal(recipes_text, "rcName", "nama resep")
    craft_rows = []
    for index, recipe in enumerate(recipes):
        suffix = "  [butuh blueprint]" if recipe["blueprint"] else (
            "  [bahan kurang]" if index % 3 else "")
        craft_rows.append(f"{recipe['name']}  ({recipe['ingredient']}){suffix}")
    jobs.append(("08-dialog-craft", "Dialog meja crafting",
                 render_dialog("Meja crafting", craft_rows, ("Buat", "Tutup"),
                               note="dialog native SA-MP, isi dari config/recipes.inc"),
                 BACKDROP_DIALOG))

    admin_rows = [
        "Pemain & akun\tKebal, uang, XP, item, ban (pilih orangnya dulu)",
        "Zombie\tSpawn per tipe, jumlah, bersihkan, boss",
        "Item\tBeri item per kategori (tanpa mengetik id)",
        "Diri sendiri\tKebal, pulihkan, teleport hub",
        "Dunia & event\tJam, supply drop, blood moon, horde",
        "Teleport\tTujuan populer tanpa mengetik koordinat",
        "Moderasi\tDaftar ban aktif, buka ban",
        "Server\tRingkasan beban + selftest",
    ]
    require_literal(panel_text, r"Dunia & event\tJam, supply drop, blood moon, horde", "baris panel admin")
    require_literal(panel_text, r"Server\tRingkasan beban + selftest", "baris panel admin")
    jobs.append(("09-dialog-admin", "Superadmin panel",
                 render_dialog("Superadmin Panel", admin_rows, ("Buka", "Tutup"),
                               header=("Menu", "Isi"),
                               note="dialog native SA-MP (TABLIST_HEADERS), isi dari admin/panel.inc"),
                 BACKDROP_DIALOG))

    menu_rows = [
        "Tas & pakai item",
        "Skill tree (7 poin)",
        "Meja crafting",
        "Stash kamp (34/120 slot)",
        "Pedagang",
        "Base: bangun / info",
        "Shelter: masuk",
        "Ambil loot di dekat",
        "Interaksi terdekat",
        "Zona & sebaran zombie",
        "Bantuan",
        "Ganti lobby (Bandara Lama)",
        "Kontrak harian (2 siap)",
    ]
    require_literal(menu_text, "Zona & sebaran zombie", "baris menu")
    require_literal(menu_text, "Kontrak harian (%d siap)", "baris menu")
    jobs.append(("10-dialog-menu", "Menu pusat",
                 render_dialog("LAST STAND - level 7, poin skill 7", menu_rows, ("Pilih", "Tutup"),
                               note="dialog native SA-MP, isi dari ui/menu.inc"),
                 BACKDROP_DIALOG))

    written: list[Path] = []
    failures = 0
    for name, title, body, backdrop in jobs:
        html = write_page(name, title, body, backdrop)
        png = OUT / f"{name}.png"
        if shoot_pngs:
            ok = shoot(html, png)
            failures += 0 if ok else 1
            print(f"  {'ok ' if ok else 'GAGAL'} {png.relative_to(ROOT)}")
        else:
            print(f"  html {html.relative_to(ROOT)}")
        written.append(png)

    print(f"\n{len(jobs)} halaman, {failures} gagal, keluaran di {OUT.relative_to(ROOT)}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
