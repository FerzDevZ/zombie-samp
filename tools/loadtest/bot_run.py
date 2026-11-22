#!/usr/bin/env python3
"""Swarm bot SA-MP 0.3.7 untuk load test LAST STAND (protokol RakNet asli).

Pakai PyRakSamp (MIT, vendored di vendor/PyRakSamp). Setiap bot:
connect -> /masuk (chat) -> jawab dialog CHARSEL -> spawn di kamp -> sync idle.
Jalankan dengan venv bcrypt (seed) — runner sendiri hanya stdlib:

  .ltvenv/bin/python tools/loadtest/bot_run.py --count 100 --duration 300

Pantau server: grep PERF server_log.txt (tick 11 sistem) dan /diag in-game.
"""
import argparse
import json
import os
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "vendor", "PyRakSamp"))

from core import logger  # noqa: E402

# Diamkan log per-bot (banner ASCII x100 tidak berguna di swarm).
logger.raw = lambda *a, **k: None
logger.info = lambda *a, **k: None
logger.debug = lambda *a, **k: None
logger.warn = lambda *a, **k: None

from bot import Bot  # noqa: E402
from samp.sync import PlayerSyncScheduler  # noqa: E402

BOT_PASSWORD_FILE = os.path.join(HERE, ".bot_password")
with open(BOT_PASSWORD_FILE, encoding="utf-8") as _pw_handle:
    BOT_PASSWORD = _pw_handle.read().strip()
CHARSEL_DIALOG = 2003
HUB_X, HUB_Y, HUB_Z = 1685.0, -2333.0, 13.0
# Di luar radius hub (180 m) tapi masih di zona tier 1 "Camp Outskirts":
# satu-satunya cara mengukur AI zombie tanpa memalsukan hasil.
CAMP_X, CAMP_Y, CAMP_Z = 1780.0, -2160.0, 13.0
# Zona non-hub: bot disebar agar streaming NPC tidak menumpuk di satu titik.
# Kepadatan menentukan berapa NPC yang di-stream ke berapa pemain — persis
# variabel yang mengubah biaya sync NPC di server.
ZONE_TARGETS = [
    (1900.0, -1200.0, 20.0),   # Idlewood
    (2200.0, -2000.0, 13.0),   # Ganton
    (1780.0, -2160.0, 13.0),   # Camp Outskirts
    (1300.0, -1700.0, 20.0),   # Downtown LS
    (-2100.0, -2400.0, 30.0),  # Angel Pine
    (400.0, 2450.0, 16.0),     # Abandoned Airfield
    (-500.0, 1400.0, 20.0),    # Bone County Quarry
    (1350.0, -1600.0, -20.0),  # LS Sewers
]


class BotRunner(threading.Thread):
    def __init__(self, nick, host, port, fps, stop_event, walk_out=True, jitter=(0.0, 0.0),
                 zone_target=None, drain_delay=0.0):
        super().__init__(daemon=True, name=nick)
        # Putus bersamaan 100 klien membuat samp03svr + streamer balapan saat
        # menghapus pemain (crash _IO_vsprintf). Klien asli jarang drop serempak,
        # jadi harness meniru disconnect bertahap.
        self.drain_delay = drain_delay
        self.walk_out = walk_out
        self.jitter_x, self.jitter_y = jitter
        self.zone_target = zone_target
        self.nick = nick
        self.host = host
        self.port = port
        self.fps = fps
        self.stop_event = stop_event
        self.state = "init"
        self.error = None
        self.bot = None
        self.spawn_seconds = None
        self.spawned_ever = False

    def run(self):
        started = time.time()
        try:
            scheduler = PlayerSyncScheduler(rate_player=0.05)  # ~20 sync/dtk, seperti client asli
            self.bot = Bot(self.host, self.port, self.nick,
                           sync_scheduler=scheduler, fps=self.fps)
            self.state = "connecting"
            if not self.bot.connect():
                self.state = "connect_failed"
                return
            self.state = "connected"
            time.sleep(1.0)
            self.bot.api.sendchat("/masuk " + BOT_PASSWORD)
            self.state = "auth_sent"

            deadline = time.time() + 45
            answered = False
            retries = 0
            last_retry = time.time()
            while time.time() < deadline and not self.stop_event.is_set():
                dialog = self.bot.api.GetDialog()
                if dialog and int(dialog.get("dialog_id", -1)) == CHARSEL_DIALOG:
                    self.bot.api.SendDialogResponse(CHARSEL_DIALOG, 1, 0, "")
                    answered = True
                    self.state = "charsel_answered"
                    break
                # Retry /masuk bila CHARSEL tak muncul (paket chat hilang /
                # lookup lambat di server lama tanpa pending-login).
                if time.time() - last_retry > 10 and retries < 3:
                    self.bot.api.sendchat("/masuk " + BOT_PASSWORD)
                    retries += 1
                    last_retry = time.time()
                time.sleep(0.25)
            if not answered:
                self.state = "no_charsel"
                return

            # Server memanggil SpawnPlayer sendiri setelah karakter dipilih,
            # jadi RPC_RequestSpawn dari server tidak pernah datang. Tanpa RPC
            # SPAWN dari klien, bot tetap di 0,0,0 dan tidak pernah terlihat
            # zombie (inilah sebab semua soak lama mengukur pemain hantu).
            time.sleep(1.5)
            for _ in range(3):
                try:
                    self.bot.session.send_rpc_spawn()
                except Exception:  # noqa: BLE001 - lanjut walau RPC gagal
                    pass
                time.sleep(0.5)

            # Bot tidak pernah menerima posisi spawn dari server: tanpa
            # setbotposition, semua sync OnFoot keluar dari 0,0,0 dan server
            # melihat pemain di tengah laut (zombie mustahil menargetkan).
            try:
                self.bot.api.setbotposition(HUB_X, HUB_Y, HUB_Z)
            except Exception as exc:  # noqa: BLE001
                self.error = "setpos:" + repr(exc)

            # Jalan keluar hub supaya zombie benar-benar punya sasaran.
            if self.walk_out:
                try:
                    target_x, target_y, target_z = self.zone_target or (CAMP_X, CAMP_Y, CAMP_Z)
                    self.bot.api.MoveToCoord(
                        target_x + self.jitter_x,
                        target_y + self.jitter_y,
                        target_z,
                        mode="foot",
                        completion_distance=2.0,
                    )
                except Exception as exc:  # noqa: BLE001
                    self.error = "move:" + repr(exc)

            deadline = time.time() + 30
            while time.time() < deadline and not self.stop_event.is_set():
                # Library tidak melacak posisi/health dari spawn RPC server;
                # sinyal spawn = CHARSEL dijawab + koneksi bertahan 8 detik
                # (server SpawnPlayer sinkron setelah SelectCharacter).
                # Verifikasi silang: characters.last_seen_at di DB.
                time.sleep(8)
                if self.bot.api.is_connected():
                    self.state = "spawned"
                    # spawned_ever tidak pernah turun walau bot putus di menit
                    # terakhir: gate harness mengukur keberhasilan spawn, bukan
                    # jumlah sesi yang kebetulan masih hidup saat laporan ditulis.
                    self.spawned_ever = True
                    self.spawn_seconds = round(time.time() - started, 1)
                    break
            if self.state != "spawned":
                self.state = "no_spawn"
                return

            while not self.stop_event.is_set():
                time.sleep(1.0)
                if not self.bot.api.is_connected():
                    self.state = "disconnected"
                    return
                try:
                    self.last_position = self.bot.api.getbotposition()
                except Exception:  # noqa: BLE001 - posisi hanya diagnostik
                    pass
        except Exception as exc:  # noqa: BLE001 - laporkan, jangan jatuh swarm
            self.error = repr(exc)
            self.state = "error"
        finally:
            if self.drain_delay > 0 and self.stop_event.is_set():
                time.sleep(self.drain_delay)
            if self.bot is not None:
                try:
                    self.bot.close()
                except Exception:  # noqa: BLE001
                    pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=10)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7777)
    parser.add_argument("--duration", type=int, default=120, help="detik idle setelah spawn")
    parser.add_argument("--fps", type=float, default=30.0)
    parser.add_argument("--stagger", type=float, default=0.4, help="detik antar connect")
    parser.add_argument("--prefix", default="LoadBot")
    parser.add_argument("--report", default=os.path.join(HERE, "last_report.json"))
    parser.add_argument("--walk-out", type=int, default=1,
                        help="1 = bot jalan keluar hub agar zombie punya sasaran")
    parser.add_argument("--spread-zones", type=int, default=0,
                        help="0 = semua bot ke satu zona, 1 = sebar ke 8 zona")
    args = parser.parse_args()

    stop_event = threading.Event()
    runners = []
    print(f"[SWARM] {args.count} bot -> {args.host}:{args.port}, stagger {args.stagger}s", flush=True)
    for index in range(1, args.count + 1):
        nick = f"{args.prefix}_{index:03d}"
        spread = (index % 7) - 3
        zone_target = None
        if args.spread_zones:
            zone_target = ZONE_TARGETS[(index - 1) % len(ZONE_TARGETS)]
        runner = BotRunner(nick, args.host, args.port, args.fps, stop_event,
                           walk_out=bool(args.walk_out),
                           jitter=(spread * 3.0, spread * 2.0),
                           zone_target=zone_target,
                           drain_delay=(index - 1) * 0.15)
        runners.append(runner)
        runner.start()
        time.sleep(args.stagger)

    connect_window = time.time() + 120
    while time.time() < connect_window:
        done = sum(1 for r in runners if r.state in ("spawned", "error", "connect_failed", "no_charsel", "no_spawn"))
        spawned = sum(1 for r in runners if r.state == "spawned")
        print(f"[SWARM] spawn {spawned}/{args.count} (selesai proses: {done})", flush=True)
        if done >= args.count:
            break
        time.sleep(5)

    print(f"[SWARM] idle {args.duration}s — pantau [PERF] di server_log.txt", flush=True)
    idle_end = time.time() + args.duration
    while time.time() < idle_end and not stop_event.is_set():
        time.sleep(5)

    stop_event.set()
    # Disconnect bertahap butuh sampai ~15 s; beri jeda itu sebelum menutup
    # laporan, lalu join singkat. Menunggu 100 thread satu per satu membuat
    # proses ini hidup lebih lama dari jendela soak dan laporannya tidak pernah
    # ditulis (gate "spawn bot" jadi 0 palsu).
    max_drain = max((r.drain_delay for r in runners), default=0.0)
    time.sleep(min(20.0, max_drain + 1.5))
    for runner in runners:
        runner.join(timeout=3)

    states = {}
    for runner in runners:
        states[runner.state] = states.get(runner.state, 0) + 1
    report = {
        "count": args.count,
        "states": states,
        "spawned": states.get("spawned", 0),
        "spawned_ever": sum(1 for r in runners if r.spawned_ever),
        "errors": [{"nick": r.nick, "state": r.state, "error": r.error}
                   for r in runners if r.error or r.state not in ("spawned",)],
        "positions": [r.last_position for r in runners if getattr(r, "last_position", None)],
        "spawn_seconds_avg": round(
            sum(r.spawn_seconds for r in runners if r.spawn_seconds) /
            max(1, sum(1 for r in runners if r.spawn_seconds)), 2),
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
    }
    with open(args.report, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2)
    print(json.dumps(report, indent=2), flush=True)
    sys.exit(0 if report["spawned_ever"] >= args.count * 0.9 else 1)


if __name__ == "__main__":
    main()
