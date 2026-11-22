#!/usr/bin/env python3
"""Uji fungsional BBM & drop lewat klien RakNet asli (tanpa GUI).

Skenario:
  1. login bot, pilih karakter, spawn di hub
  2. /bensin jalan kaki  -> laporan status + jarak pom
  3. pindah ke pom, /isi -> dialog menu pom harus muncul (id 2973)
  4. pilih baris isi 10 L -> chat konfirmasi + harga
  5. /drop 1              -> drop item memakai pickup koper (server log)
Keluaran: daftar pesan chat + id dialog yang diterima.
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, "/home/boxd/zombie-samp/tools/loadtest/vendor/PyRakSamp")

from core import logger

CHAT = []
logger.chat = lambda msg, *a, **k: CHAT.append(str(msg))
for name in ("raw", "info", "debug", "warn", "gametext", "audio"):
    setattr(logger, name, lambda *a, **k: None)

from bot import Bot
from samp.sync import PlayerSyncScheduler

PASSWORD = open("/home/boxd/zombie-samp/tools/loadtest/.bot_password", encoding="utf-8").read().strip()
HUB = (1685.0, -2333.0, 13.0)
PUMP = (1681.0, -2322.0, 13.5)
CHARSEL = 2003
PUMP_DIALOG = 2973


def wait(seconds):
    end = time.time() + seconds
    while time.time() < end:
        time.sleep(0.2)


def main():
    bot = Bot("127.0.0.1", 7777, "loadbot_001", sync_scheduler=PlayerSyncScheduler(rate_player=0.05), fps=30)
    if not bot.connect():
        print("GAGAL connect")
        return 1

    wait(1.2)
    bot.api.sendchat("/masuk " + PASSWORD)

    deadline = time.time() + 40
    while time.time() < deadline:
        dialog = bot.api.GetDialog()
        if dialog and int(dialog.get("dialog_id", -1)) == CHARSEL:
            bot.api.SendDialogResponse(CHARSEL, 1, 0, "")
            break
        time.sleep(0.25)

    # cache dialog harus dikosongkan: GetDialog() mengembalikan dialog terakhir yang
    # pernah dilihat, bukan "dialog yang sedang terbuka".
    bot.api.session.chat.last_dialog = None

    wait(2.0)
    for _ in range(3):
        try:
            bot.session.send_rpc_spawn()
        except Exception:
            pass
        wait(0.4)

    bot.api.setbotposition(*HUB)
    wait(1.5)

    print("== cek login: /inv")
    CHAT.clear()
    bot.api.sendchat("/inv")
    wait(1.5)
    for line in CHAT:
        print("   ", line)

    print("== /bensin di hub (jalan kaki)")
    CHAT.clear()
    bot.api.sendchat("/bensin")
    wait(2.0)
    for line in CHAT:
        print("   ", line)

    print("== pindah ke pom lalu /isi")
    bot.api.setbotposition(*PUMP)
    wait(1.5)
    CHAT.clear()
    bot.api.sendchat("/isi")
    wait(2.0)

    dialog = bot.api.GetDialog()
    dialog_id = int(dialog.get("dialog_id", -1)) if dialog else -1
    print("    dialog id:", dialog_id, "(harap", PUMP_DIALOG, ")")
    if dialog:
        body = dialog.get("body") or dialog.get("text") or ""
        print("    isi menu:")
        for row in str(body).splitlines():
            print("      |", row)

    if dialog_id == PUMP_DIALOG:
        bot.api.SendDialogResponse(PUMP_DIALOG, 1, 1, "")
        wait(1.5)
        print("    respons pilih baris 2:")
        for line in CHAT:
            print("   ", line)

    print("== /drop 1 (visual drop)")
    CHAT.clear()
    bot.api.sendchat("/drop 1")
    wait(1.5)
    for line in CHAT:
        print("   ", line)

    return 0


if __name__ == "__main__":
    sys.exit(main())
