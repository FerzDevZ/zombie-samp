#if defined _ls_constants_included
    #endinput
#endif
#define _ls_constants_included

/* ---------------------------------------------------------------- limits */
#define MAX_ZOMBIES 190
/* Saklar diagnostik: 1 = tidak membuat zombie sama sekali.
   Dipakai untuk memisahkan biaya memori "per pemain" dari "per NPC"
   saat mengukur kebocoran heap (lihat docs/RANCANGAN_2026.md §4 O2). */
#define ZOMBIE_SPAWN_DISABLED 0
#define MAX_ZOMBIE_SLOTS 512
#define ZOMBIE_TICK_MS 200
#define ZOMBIE_AMBIENT_BUDGET 40
#define ZOMBIE_HORDE_BUDGET 150
#define MAX_ZOMBIE_TYPES 8

/* Boss malam (ZT_BOSS / Abomination): satu per lobi, hanya malam, reward sekali. */
#define ZOMBIE_BOSS_REWARD_CASH 250
#define ZOMBIE_BOSS_REWARD_XP   60
#define BOSS_SPAWN_CHANCE       40
#define BOSS_RESPAWN_MS         900000
#define BOSS_SPAWN_RADIUS       55.0
#define BOSS_MIN_PLAYER_DIST    30.0
/* Batas detail audit log admin (tabel admin_actions, migrasi 022). */
#define ADMIN_AUDIT_DETAIL_MAX 180

#define ZOMBIE_BOSS_PHASE2_PERCENT 50
#define ZOMBIE_BOSS_PHASE2_ADDS    5

#define ZOMBIE_LOD_FULL_DIST        (45.0)
#define ZOMBIE_LOD_CHASE_DIST       (90.0)
#define ZOMBIE_ATTACK_RANGE         (2.6)

/* FCNPC stops this far from the player. It must stay inside ZOMBIE_ATTACK_RANGE or
   the zombie parks at 4 m and never lands a hit. */
#define ZOMBIE_APPROACH_DIST        (1.6)
#define ZOMBIE_ATTACK_COOLDOWN_MS   (1200)
/* 1 = perintah uji (/test, /noise, /hurt, /infect, /heal) terbuka untuk semua pemain.
   Wajib 0 sebelum server dibuka ke publik. */
#define DEV_MODE 0

/* 0 = langkah input (password, nama karakter) hanya lewat chat command.
   1 = tampilkan dialog INPUT seperti versi lama.

   Kenapa default 0: klien SA-MP Mobile kerap mengirim `inputtext` kosong dari
   dialog INPUT, sehingga server melihat "password kosong" padahal pemain sudah
   mengetik. Jalur chat memakai `params` command yang selalu terkirim utuh. */
#define AUTH_INPUT_DIALOGS 0

#define ZOMBIE_NOISE_MEMORY_MS 20000
#define ZOMBIE_REPATH_MS            (2200)
#define ZOMBIE_CHASE_REPATH_MS      (1800)
#define ZOMBIE_REPATH_MIN_MOVE      (1.5)
#define ZOMBIE_DIRECT_PATH_DIST     (30.0)
/* 1 = zombie memakai pathfinding raycast ColAndreas (mahal, default),
   0 = hanya jalan lurus + LOS. Diukur lewat mem_probe: lihat docs/PLAN_NEXT.md. */
#define ZOMBIE_PATHFINDING_RAYCAST  (1)
#define ZOMBIE_STUCK_CHECK_MS       (2000)
#define ZOMBIE_STUCK_MIN_MOVE       (1.0)
#define ZOMBIE_MIN_SPAWN_DIST       (35.0)
#define ZOMBIE_MAX_SPAWN_DIST       (140.0)

/* Budget aktif mengikuti jumlah pemain. 190 NPC untuk satu pemain membuat
   FCNPC mengirim sync ribuan kali per detik dan animasi terlihat tersendat. */
#define DIRECTOR_BUDGET_BASE        (18)
#define DIRECTOR_BUDGET_PER_PLAYER  (34)
#define DIRECTOR_BUDGET_MAX         (150)
#define DIRECTOR_ZONE_TARGET_MAX    (8)
#define ZOMBIE_TRIM_MIN_DIST        (70.0)
#define FCNPC_SYNC_RATE_MS          (100)
#define FCNPC_SYNC_RATE_CROWD_MS    (250)
#define FCNPC_CROWD_THRESHOLD       (25)
/* Kepadatan = berapa pemain di zona terpadat. Biaya sync NPC tumbuh sebagai
   pemain x NPC yang ter-stream, jadi zona padat wajib diperlambat. */
#define FCNPC_SYNC_RATE_DENSE_MS    (400)
#define FCNPC_DENSE_THRESHOLD       (45)
/* Plafon NPC ambient saat satu zona menampung >= FCNPC_DENSE_THRESHOLD pemain:
   sync tiap NPC dikirim ke tiap pemain di dekatnya, jadi kepadatan = biaya. */
#define ZOMBIE_DENSE_CAP            (45)

/* ---------------------------------------------------------------- spatial */
#define HASH_CELL_SIZE              (100.0)
#define HASH_CELLS 60
#define HASH_ZOMBIE_SLOTS 3600
#define HASH_PLAYER_CELLS 30
#define HASH_PLAYER_SLOTS 900
#define HASH_MIN_COORD              (-3000.0)

/* ---------------------------------------------------------------- survival */
#define MAX_INVENTORY_SLOTS 20
#define AUTOSAVE_INTERVAL_MS 120000
#define AUTOSAVE_TICK_MS (10000)
#define AUTOSAVE_BATCH_SIZE (5)
/* 60 slot dasar + 20 per crate (maks 120). Array memori disize ke batas atas
   supaya bonus crate tidak pernah menulis di luar batas (bug lama: array 60
   tapi kapasitas efektif 120). */
#define STASH_BASE_SLOTS 60
#define MAX_STASH_SLOTS  120

/* --- Anti-cheat: deteksi, bukan larangan buta --- */
#define AC_SAMPLE_INTERVAL_MS 1000
#define AC_MAX_FOOT_SPEED    20.0
#define AC_TELEPORT_DIST     40.0
/* Kendaraan: ambang lebih tinggi (mobil sah ~200 km/j = 55 m/s); sebelumnya
   DRIVER/PASSENGER dilewati total = speedhack kendaraan tak terdeteksi (F-14). */
#define AC_MAX_VEHICLE_SPEED 75.0
#define AC_TELEPORT_DIST_VEH 120.0
#define AC_FLAG_WINDOW_MS    120000
#define AC_WARN_FREEZE_MS    2500
#define AC_KICK_THRESHOLD    6
#define AC_RAPID_TOLERANCE_MS 20

/* --- Barricade persisten --- */
#define MAX_BARRICADES       40
#define BARRICADE_MODEL      1225
#define BARRICADE_MAX_HEALTH 1500
#define BARRICADE_ZOMBIE_DPS 15
#define BARRICADE_REPAIR_SCRAP 2
#define BARRICADE_REPAIR_HEALTH 400

/* --- Horde director --- */
#define HORDE_TICK_MS        5000
#define HORDE_HEAT_DECAY     0.90
#define HORDE_HEAT_TRIGGER   45.0
#define HORDE_BASE_RAID_COOLDOWN_MS 600000
#define HORDE_BASE_RAID_RADIUS      45.0
#define HORDE_BASE_RAID_MIN         8
#define HORDE_BASE_RAID_MAX         14
#define HORDE_COOLDOWN_MS    45000

/* Raid base terarah: heat dihitung PER BASE (bukan per zona) supaya gerombolan
   menyerang base yang benar-benar berisik, bukan seluruh zona. */
#define BASE_HEAT_RADIUS          26.0
#define BASE_HEAT_GAIN_DIV        6.0
#define BASE_HEAT_DECAY           0.94
#define BASE_RAID_MIN_HEAT        30.0
#define BASE_RAID_LEASH           38.0
#define BASE_RAID_HOLD_DIST       3.5
#define BASE_RAID_REWARD_CASH     420
#define BASE_RAID_REWARD_XP       35
#define BASE_RAID_TIMEOUT_MS      180000
#define HORDE_CLUSTER_MIN    6
#define HORDE_CLUSTER_MAX    14
#define SCREAMER_PULSE_MS    20000
#define SCREAMER_RANGE       50.0
#define WEAPON_CONDITION_MAX 100
#define WEAPON_CONDITION_PER_SHOT 1
#define ITEM_DURABILITY_USE_COST 20
#define ITEM_BANDAGE_HEAL 15.0
#define ITEM_MEDKIT_HEAL 50.0
#define ITEM_BLOODPACK_HEAL 100.0
#define ITEM_ANTIBIOTICS_CURE 30.0
#define MAX_INVENTORY_WEIGHT        (30.0)
#define SPAWN_X 1685.0
#define SPAWN_Y -2333.0
#define SPAWN_Z 13.0
#define SPAWN_ANGLE 90.0
/* Grace newbie 10 menit sesuai DESIGN §14 (audit N3): sebelumnya 15 detik —
   terlalu singkat untuk proteksi PvP + needs decay pemain baru. */
#define PLAYER_GRACE_MS 600000

#define HUB_ZONE_ID 1
#define FULL_HP                     (100.0)
#define INFECTION_TURNED_MS         (90000)
#define INFECTION_CONTROL_MS        (3000)
#define REVIVE_RANGE                (3.0)

/* ---------------------------------------------------------------- palette */
#define UI_BG_GLASS_DARK            0x0B0F19E6
#define UI_BG_SURFACE               0x111827F0
#define UI_BG_ELEVATED              0x1E293BF2
#define UI_TEXT_PRIMARY             0xF8FAFCFF
#define UI_TEXT_SECONDARY           0x94A3B8FF
#define UI_TEXT_MUTED               0x64748BFF
#define UI_ACCENT_PRIMARY           0x10B981FF
#define UI_ACCENT_INFO              0x38BDF8FF
#define UI_ACCENT_WARN              0xF59E0BFF
#define UI_ACCENT_DANGER            0xEF4444FF
#define UI_ACCENT_INFECTION         0x84CC16FF
#define TD_ASPECT_16_9              (0.75)

/* ---------------------------------------------------------------- misc */
#define NOISE_SHOT_PISTOL           (45.0)
#define NOISE_SHOT_SMG              (60.0)
#define NOISE_SHOT_SHOTGUN          (75.0)
#define NOISE_SHOT_RIFLE            (90.0)
#define NOISE_SUPPRESSED            (12.0)
/* Suppressor bertahan 40 tembakan pistol (durability item = sisa tembakan). */
#define SUPPRESSOR_SHOTS            40
/* Channel revive 6 detik: bisa diinterupsi damage/jarak (bukan instan). */
#define REVIVE_CHANNEL_MS           6000
#define NOISE_MELEE                 (0.0)

#define MAX_ZONE_NAME 32
#define MAX_ZONE_COUNT 10
#define INVALID_ZOMBIE              (-1)
#define INVALID_ZONE                (-1)
