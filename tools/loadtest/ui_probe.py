#!/usr/bin/env python3
"""Diagnostik UI/kendaraan lewat klien RakNet: /uicheck sebelum & sesudah /veh.

Pemain melaporkan "HUD tidak ada fungsinya dan tidak ada mobil". Skrip ini
membuktikan dari sisi server apa yang seharusnya tampil: status HUD, lobi,
dunia virtual, jumlah mobil liar di lobi, jarak mobil terdekat, jumlah peti.
"""
import os
import sys
import time

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
CHARSEL = 2003


def wait(seconds):
    end = time.time() + seconds
    while time.time() < end:
        time.sleep(0.2)


def show(label):
    print(label)
    for line in CHAT:
        print("   ", line)
    CHAT.clear()


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
    bot.api.session.chat.last_dialog = None

    wait(2.0)
    for _ in range(3):
        try:
            bot.session.send_rpc_spawn()
        except Exception:
            pass
        wait(0.4)

    bot.api.setbotposition(*HUB)
    wait(2.5)

    CHAT.clear()
    bot.api.sendchat("/uicheck")
    wait(2.0)
    show("== /uicheck setelah spawn")

    print("== tunggu 22 detik: tick kendaraan liar 20 s harus mengisi mobil dekat pemain")
    wait(22.0)
    CHAT.clear()
    bot.api.sendchat("/uicheck")
    wait(2.0)
    show("== /uicheck setelah tick kendaraan")

    CHAT.clear()
    bot.api.sendchat("/masuk saya mau lihat kendaraan")
    wait(1.5)
    show("== cek apakah chat diproses (bukan perintah)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
