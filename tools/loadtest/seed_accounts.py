#!/usr/bin/env python3
"""Seed akun+character bot untuk load test (HANYA DB dev).

Bot login lewat /masuk (chat) seperti client asli; akun di-seed langsung ke DB
agar tidak menabrak cap registrasi 3 akun/IP/24 jam. Password test tetap,
bukan kredensial produksi. Jalankan dengan venv yang punya bcrypt:

  python3 -m venv .ltvenv && .ltvenv/bin/pip install bcrypt
  .ltvenv/bin/python tools/loadtest/seed_accounts.py --count 100
"""
import argparse
import os
import secrets
import subprocess
import sys
import time

try:
    import bcrypt
except ImportError:
    sys.exit("bcrypt belum ada: python3 -m venv .ltvenv && .ltvenv/bin/pip install bcrypt")

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PASSWORD_FILE = os.path.join(os.path.dirname(__file__), ".bot_password")


def load_or_create_password():
    """Password bot dibuat runtime (bukan literal di kode sumber) dan disimpan
    di file gitignore .bot_password — seed dan runner membaca sumber yang sama."""
    if os.path.exists(PASSWORD_FILE):
        with open(PASSWORD_FILE, encoding="utf-8") as handle:
            return handle.read().strip()
    password = secrets.token_urlsafe(12)
    with open(PASSWORD_FILE, "w", encoding="utf-8") as handle:
        handle.write(password + "\n")
    os.chmod(PASSWORD_FILE, 0o600)
    return password


def read_db_password():
    ini = os.path.join(ROOT, "mysql.ini")
    with open(ini, encoding="utf-8") as handle:
        for line in handle:
            if line.strip().startswith("password"):
                return line.split("=", 1)[1].strip()
    sys.exit("password tidak ditemukan di mysql.ini")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--prefix", default="LoadBot")
    parser.add_argument("--container", default="laststand-mysql")
    parser.add_argument("--db", default="laststand")
    parser.add_argument("--user", default="laststand")
    parser.add_argument("--spread", action="store_true",
                        help="sebar bot 11+ ke zona non-hub (load test nyata)")
    args = parser.parse_args()

    db_pass = read_db_password()
    bot_password = load_or_create_password()
    pw_hash = bcrypt.hashpw(bot_password.encode(), bcrypt.gensalt(rounds=12)).decode()
    now_ms = int(time.time() * 1000)

    statements = []
    # Zona non-hub untuk load test nyata: bot spawn di luar kamp sehingga
    # needs decay, zombie, event (supply drop/blood moon), dan kematian aktif.
    spread_zones = [
        (1, 1900.0, -1200.0, 20.0),   # Idlewood
        (2, 2200.0, -2000.0, 13.0),   # Ganton
        (3, 1300.0, -1700.0, 20.0),   # Downtown LS
        (5, -2100.0, -2400.0, 30.0),  # Angel Pine
        (6, 400.0, 2450.0, 16.0),     # Abandoned Airfield
        (7, -500.0, 1400.0, 20.0),    # Bone County Quarry
        (8, 1780.0, -2160.0, 13.0),   # Camp Outskirts
    ]
    import random
    rng = random.Random(20261119)
    for index in range(1, args.count + 1):
        nick = f"{args.prefix}_{index:03d}"
        username = nick.lower()
        statements.append(
            "INSERT INTO accounts (username, password_hash, created_at, last_ip) "
            f"VALUES ('{username}', '{pw_hash}', {now_ms}, '127.0.0.1') "
            "ON DUPLICATE KEY UPDATE password_hash=VALUES(password_hash);"
        )
        statements.append(
            f"SET @aid = (SELECT id FROM accounts WHERE username = '{username}');"
        )
        if args.spread and index > 10:
            zone_id, zx, zy, zz = spread_zones[index % len(spread_zones)]
            px = zx + rng.uniform(-40.0, 40.0)
            py = zy + rng.uniform(-40.0, 40.0)
            pos_sql = (f"pos_x={px:.2f}, pos_y={py:.2f}, pos_z={zz:.2f}, "
                       f"zone_id={zone_id}")
            insert_pos = (f", pos_x, pos_y, pos_z, zone_id) VALUES (@aid, 0, '{nick}', 29, 1, 500, "
                          f"{px:.2f}, {py:.2f}, {zz:.2f}, {zone_id})")
        else:
            pos_sql = "pos_x=1685.0, pos_y=-2333.0, pos_z=13.0, zone_id=0"
            insert_pos = ") VALUES (@aid, 0, '%s', 29, 1, 500)" % nick
        statements.append(
            "INSERT INTO characters (account_id, slot, name, skin, level, cash"
            + insert_pos +
            " ON DUPLICATE KEY UPDATE skin=29, " + pos_sql + ";"
        )

    sql = "\n".join(statements) + "\n"
    proc = subprocess.run(
        ["docker", "exec", "-i", args.container, "mariadb",
         f"-u{args.user}", f"-p{db_pass}", args.db],
        input=sql.encode(), capture_output=True,
    )
    if proc.returncode != 0:
        sys.exit(f"seed gagal: {proc.stderr.decode()[:400]}")
    print(f"[SEED] {args.count} akun+character {args.prefix}_001..{args.prefix}_{args.count:03d} siap (password di .bot_password)")


if __name__ == "__main__":
    main()
