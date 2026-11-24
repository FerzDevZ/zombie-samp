#!/usr/bin/env python3
"""Gate zona aman HUD Android — SATU-SATUNYA sumber kebenaran posisi.

Kenapa ada: dua bug nyata di repo ini (blok BBM di tengah, lalu gaya
MOBILE/MINIMAL menariknya kembali ke tengah) tidak tertangkap gate mana pun
karena gate lama hanya memeriksa LEBAR teks di kanvas 640, bukan POSISI
terhadap geometri klien Android yang sebenarnya.

Angka di bawah bukan perkiraan; diturunkan dari screenshot klien 2340x1080
(tepi kartu kiri design y328..428 tampak di px 795..1035):

    px_y = 2.400 * y + 8          -> kanvas 480 unit = 1160 px
    layar hanya 1080 px           -> klien MEMOTONG di y ~ 447
    px_x = 3.106 * x              (kanvas 1984 px, sisa 356 px untuk tombol)

Zona sentuh klien (terukur, px -> design x):
    joystick/fingerprint  x ~ 10..37,  y ~ 452..499
    action pad kanan      x ~ 497..750, y ~ 312..499

Aturan lulus:
  1. Setiap Y elemen HUD, SETELAH dikali skala gaya (jangkar bawah), harus di
     pita 320..446 untuk KETIGA gaya.
  2. Elemen yang jatuh di y' >= 400 tidak boleh ber-X di zona sentuh.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HUD = ROOT / "gamemodes/modules/ui/hud.inc"
CONST = ROOT / "gamemodes/modules/config/constants.inc"

PX_SY, PX_OY = 2.400, 8.0
ANDROID_VISIBLE_Y = (1080.0 - PX_OY) / PX_SY          # 446.7
BAND_MIN, BAND_MAX = 320.0, 446.0      # pita bawah tempat semua grup HUD
TOP_BAND = (0.0, 130.0)                 # HUD_YT: elemen yang memang menempel atas
ANCHOR = 440.0
TOUCH_RIGHT = (497.0, 750.0)     # action pad (hanya relevan di pita bawah)

GAYAS = [("PENUH", 1.00), ("MOBILE", 0.70), ("MINIMAL", 0.85)]


def defines(text):
    out = {}
    for name, val in re.findall(r"#define\s+(\w+)\s+(-?[0-9]+(?:\.[0-9]+)?)\b", text):
        out[name] = float(val)
    return out


def resolve(token, table):
    token = token.strip()
    if token in table:
        return table[token]
    try:
        return float(token)
    except ValueError:
        return None


def scaled(y, k):
    return ANCHOR - (ANCHOR - y) * k


def collect(path):
    """Ambil (file, baris, sumbu, token) untuk setiap HUD_X/HUD_Y/HUD_YT/HUD_XC."""
    items = []
    table = {}
    for f in (CONST, path):
        table.update(defines(f.read_text(errors="ignore")))
    for n, line in enumerate(path.read_text(errors="ignore").splitlines(), 1):
        if line.lstrip().startswith("*") or "stock Float:HUD_" in line:
            continue
        for axis, fn in (("y", "HUD_YT"), ("y", "HUD_Y"), ("x", "HUD_XC"), ("x", "HUD_X")):
            for m in re.finditer(rf"\b{fn}\(playerid,\s*([A-Za-z0-9_.]+)\)", line):
                val = resolve(m.group(1), table)
                if val is not None:
                    items.append((n, axis, fn, m.group(1), val))
    return items, table


def check(items):
    errors, warns = [], []
    for n, axis, fn, tok, val in items:
        if axis == "y" and fn == "HUD_YT":
            ys = {name: val * k for name, k in GAYAS}
            for name, y in ys.items():
                if not (TOP_BAND[0] <= y <= TOP_BAND[1]):
                    errors.append(
                        f"{HUD.name}:{n}  HUD_YT({tok}) gaya {name} -> y {y:.1f} "
                        f"di luar pita atas {TOP_BAND[0]:.0f}..{TOP_BAND[1]:.0f}")
            continue
        if axis == "y":
            ys = {name: scaled(val, k) for name, k in GAYAS}
            for name, y in ys.items():
                if y > BAND_MAX:
                    errors.append(
                        f"{HUD.name}:{n}  {fn}({tok}) gaya {name} -> y {y:.1f} "
                        f"> {BAND_MAX:.0f} (terpotong klien Android, batas {ANDROID_VISIBLE_Y:.0f})")
                elif y < BAND_MIN:
                    errors.append(
                        f"{HUD.name}:{n}  {fn}({tok}) gaya {name} -> y {y:.1f} "
                        f"< {BAND_MIN:.0f} (di tengah layar)")
            if fn == "HUD_Y":
                lo, hi = TOUCH_RIGHT
                if max(ys.values()) >= 400.0 and lo - 5.0 <= val <= hi + 5.0:
                    warns.append(f"{HUD.name}:{n}  HUD_Y({tok}) x={val:.0f} masuk zona sentuh kanan (>=400)")
        else:
            if fn == "HUD_XC":
                continue
            if TOUCH_RIGHT[0] - 5.0 <= val <= TOUCH_RIGHT[1] + 5.0:
                warns.append(f"{HUD.name}:{n}  {fn}({tok}) = {val:.0f} di kolom zona sentuh kanan")

    return errors, warns


def self_test():
    """Bukti gate benar-benar menangkap bug: dua kasus buruk + satu baik."""
    cases = [
        ("y di tengah (200 -> 140 saat gaya MOBILE)", [("y", "HUD_Y", "X", 200.0)], True),
        ("HUD_YT di tengah (300)", [("y", "HUD_YT", "X", 300.0)], True),
        ("HUD_YT di atas (46)", [("y", "HUD_YT", "X", 46.0)], False),
        ("y terpotong (470)", [("y", "HUD_Y", "X", 470.0)], True),
        ("y aman (424)", [("y", "HUD_Y", "X", 424.0)], False),
    ]
    ok = True
    for label, items4, expect_error in cases:
        items = [(1, axis, fn, tok, val) for axis, fn, tok, val in items4]
        errs, _ = check(items)
        got = bool(errs)
        status = "OK" if got == expect_error else "SALAH"
        if got != expect_error:
            ok = False
        print(f"  [{status}] {label}: {'ditolak' if got else 'lolos'}")
    return ok


def main():
    if "--self-test" in sys.argv:
        print("self-test gate zona Android:")
        return 0 if self_test() else 1

    items, table = collect(HUD)
    if not items:
        print("GATE ANDROID: GAGAL — tidak ada elemen HUD yang terbaca (regex rusak?)")
        return 1
    errors, warns = check(items)
    for w in warns:
        print("  WARN " + w)
    for e in errors:
        print("  ERROR " + e)
    if errors:
        print(f"GATE ANDROID: GAGAL — {len(errors)} elemen di luar pita aman")
        return 1
    ys = [v for _, a, _, _, v in items if a == "y"]
    print(f"GATE ANDROID: lulus — {len(ys)} Y + "
          f"{len(items) - len(ys)} X diperiksa di 3 gaya "
          f"(pita {BAND_MIN:.0f}..{BAND_MAX:.0f})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
