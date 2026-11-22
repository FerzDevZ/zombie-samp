#!/usr/bin/env python3
"""Probe crash: event dipaksa berulang SAAT churn koneksi berlangsung.

Hipotesis: crash vsprintf (format string) terjadi saat event/timer berjalan
bersamaan dengan connect/auth/disconnect pemain. Admin bot memaksa /event tiap
2 detik; 20 bot worker connect-login-idle-disconnect berulang.
"""
import os
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "vendor", "PyRakSamp"))

from core import logger  # noqa: E402
logger.raw = lambda *a, **k: None
logger.info = lambda *a, **k: None
logger.debug = lambda *a, **k: None
logger.warn = lambda *a, **k: None

from bot import Bot  # noqa: E402
from samp.sync import PlayerSyncScheduler  # noqa: E402

with open(os.path.join(HERE, ".bot_password"), encoding="utf-8") as handle:
    PASSWORD = handle.read().strip()
CHARSEL = 2003
STOP = threading.Event()


def login(bot):
    if not bot.connect():
        return False
    time.sleep(1.0)
    bot.api.sendchat("/masuk " + PASSWORD)
    deadline = time.time() + 30
    while time.time() < deadline and not STOP.is_set():
        dialog = bot.api.GetDialog()
        if dialog and int(dialog.get("dialog_id", -1)) == CHARSEL:
            bot.api.SendDialogResponse(CHARSEL, 1, 0, "")
            time.sleep(6)
            return bot.api.is_connected()
        time.sleep(0.25)
    return False


def admin_loop():
    bot = Bot("127.0.0.1", 7777, "LoadBot_120",
              sync_scheduler=PlayerSyncScheduler(rate_player=0.05), fps=30)
    if not login(bot):
        print("[PROBE] admin gagal login", flush=True)
        return
    print("[PROBE] admin siap, spam /event tiap 2s", flush=True)
    count = 0
    while not STOP.is_set():
        if not bot.api.is_connected():
            print("[PROBE] admin terputus — server mungkin crash", flush=True)
            STOP.set()
            return
        bot.api.sendchat("/event")
        count += 1
        if count % 15 == 0:
            print(f"[PROBE] {count} event dipaksa", flush=True)
        time.sleep(2)
    bot.close()


def worker_loop(index):
    nick = f"LoadBot_{index:03d}"
    cycles = 0
    while not STOP.is_set() and cycles < 30:
        bot = Bot("127.0.0.1", 7777, nick,
                  sync_scheduler=PlayerSyncScheduler(rate_player=0.05), fps=30)
        ok = login(bot)
        if ok:
            time.sleep(8)
        try:
            bot.close()
        except Exception:  # noqa: BLE001
            pass
        cycles += 1
        time.sleep(1)
    print(f"[PROBE] worker {nick} selesai ({cycles} siklus)", flush=True)


def main():
    duration = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    worker_start = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    worker_count = int(sys.argv[3]) if len(sys.argv) > 3 else 20
    threads = [threading.Thread(target=admin_loop, daemon=True)]
    for index in range(worker_start, worker_start + worker_count):
        threads.append(threading.Thread(target=worker_loop, args=(index,), daemon=True))
        time.sleep(0.3)
    for thread in threads:
        thread.start()
    end = time.time() + duration
    while time.time() < end and not STOP.is_set():
        time.sleep(2)
    STOP.set()
    print("[PROBE] selesai", flush=True)


if __name__ == "__main__":
    main()
