# Load Test Bot Client — LAST STAND

Bot SA-MP 0.3.7 berbasis RakNet asli (PyRakSamp, MIT, vendored di `vendor/PyRakSamp`).
Bot menjalankan alur pemain nyata: connect → `/masuk` (chat) → dialog CHARSEL →
spawn → sync berkala (~20/dtk seperti client asli). Tidak ada game client yang
dibutuhkan; semua dari terminal.

## Prasyarat

- Server hidup (`./scripts/run-server.sh`) + MySQL docker `laststand-mysql`.
- venv Python dengan bcrypt (hanya untuk seed):

      python3 -m venv .ltvenv
      .ltvenv/bin/pip install bcrypt

## Prosedur

1. Seed akun + character bot (password acak runtime, disimpan di `.bot_password`
   yang di-gitignore; tidak ada literal password di kode):

       .ltvenv/bin/python tools/loadtest/seed_accounts.py --count 120 --spread

   `--spread`: bot 11+ spawn di zona non-hub (Idlewood/Ganton/Downtown LS/
   Angel Pine/Airfield/Quarry/Camp Outskirts) sehingga needs decay, zombie,
   event supply-drop/blood-moon, dan alur kematian aktif. 10 bot pertama di
   kamp (hub) sebagai baseline aman.

2. Swarm penuh (100 pemain, 10 menit idle — mencakup 2 siklus event 5 menit
   dan transisi malam):

       .ltvenv/bin/python tools/loadtest/bot_run.py --count 100 --duration 600

   Laporan JSON: `tools/loadtest/last_report.json` (states, spawned,
   spawn_seconds_avg, errors). Exit 0 bila ≥90% spawn.

3. Churn + event paksa (opsional, uji race connect/auth/disconnect + event):

       .ltvenv/bin/python tools/loadtest/crash_probe.py 300 101 19

   Admin `LoadBot_120` (admin_level 5 di DB) memaksa `/event` tiap 2 detik;
   worker 101-119 connect-login-idle-disconnect berulang. Jangan jalankan
   probe dengan nick yang sedang dipakai swarm.

4. Pantau server:

       grep PERF server_log.txt | tail        # tick 11 sistem, budget 20 ms
       grep DIRECTOR server_log.txt | tail    # populasi zombie & trim
       tail logs/errors.log                   # wajib kosong

## Interpretasi

- Budget ROADMAP fase 8: p95 tick <20 ms pada 100+ pemain. `[PERF]` mencatat
  akhir/puncak per window 60 detik untuk 11 sistem (zombie, director, base,
  hud, player, loot, veh, needs, event, horde, save).
- Bot tidak melawan zombie; pemain idle di zona bahaya akan downed/mati —
  itu justru menguji alur grave/corpse/respawn di bawah beban.
- Koneksi dari 127.0.0.1: cap registrasi 3/IP/24 jam TIDAK terpicu karena
  akun di-seed langsung ke DB (bot hanya `/masuk`). Limit login per-IP hanya
  menghitung kegagalan, jadi login benar massal aman.

## Batasan

- Bot bukan client grafis: tidak memverifikasi rendering TextDraw/HUD,
  audio, atau input mobile. Uji itu tetap butuh client PC/Android asli
  (checklist `tests/REGRESSION_26-11-19.md`).
- PyRakSamp tidak melacak posisi/health dari RPC spawn server; deteksi
  spawn bot = CHARSEL dijawab + koneksi bertahan 8 detik (server
  SpawnPlayer sinkron setelah SelectCharacter). Verifikasi silang:
  `characters.last_seen_at` di DB.
- untuk uji >200 pemain: naikkan `maxplayers`/`maxnpc` di pawn.json +
  `MAX_PLAYERS` di main.pwn (jangan di bawah maxplayers+maxnpc), dan
  ingat array global ikut membesar (laporan compiler: ukuran AMX).
