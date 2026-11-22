-- LAST STAND: San Andreas Quarantine
-- 001_init.sql — skema awal (MySQL 8 / MariaDB 10.11)
-- Konvensi: InnoDB, utf8mb4, timestamp = epoch milidetik (BIGINT)
-- Catatan: file ini belum dieksekusi. Jalankan di fase 0 lalu verifikasi dengan SHOW TABLES.

SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version     VARCHAR(32)  NOT NULL,
    applied_at  BIGINT       NOT NULL,
    PRIMARY KEY (version)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS accounts (
    id            INT UNSIGNED NOT NULL AUTO_INCREMENT,
    username      VARCHAR(24)  NOT NULL,
    password_hash VARCHAR(72)  NOT NULL,           -- bcrypt
    email         VARCHAR(120) DEFAULT NULL,
    created_at    BIGINT       NOT NULL,
    last_login_at BIGINT       NOT NULL DEFAULT 0,
    last_ip       VARCHAR(45)  DEFAULT NULL,
    is_banned     TINYINT(1)   NOT NULL DEFAULT 0,
    PRIMARY KEY (id),
    UNIQUE KEY uq_accounts_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS characters (
    id             INT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id     INT UNSIGNED NOT NULL,
    slot           TINYINT UNSIGNED NOT NULL,       -- 0..1
    name           VARCHAR(24)  NOT NULL,
    skin           SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    level          SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    xp             INT UNSIGNED NOT NULL DEFAULT 0,
    cash           INT UNSIGNED NOT NULL DEFAULT 500,
    scrap          INT UNSIGNED NOT NULL DEFAULT 0,
    rank_points    INT UNSIGNED NOT NULL DEFAULT 0, -- Quarantine Rank
    infection      TINYINT UNSIGNED NOT NULL DEFAULT 0,
    health         FLOAT  NOT NULL DEFAULT 100.0,
    hunger         TINYINT UNSIGNED NOT NULL DEFAULT 100,
    thirst         TINYINT UNSIGNED NOT NULL DEFAULT 100,
    zone_id        TINYINT UNSIGNED NOT NULL DEFAULT 1,
    pos_x          FLOAT  NOT NULL DEFAULT 1685.0,
    pos_y          FLOAT  NOT NULL DEFAULT -2333.0,
    pos_z          FLOAT  NOT NULL DEFAULT 13.0,
    pos_a          FLOAT  NOT NULL DEFAULT 0.0,
    stash_capacity TINYINT UNSIGNED NOT NULL DEFAULT 60,
    last_seen_at   BIGINT NOT NULL DEFAULT 0,
    playtime_sec   INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (id),
    UNIQUE KEY uq_characters_name (name),
    UNIQUE KEY uq_characters_slot (account_id, slot),
    KEY idx_characters_account (account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS inventory (
    char_id    INT UNSIGNED NOT NULL,
    slot       TINYINT UNSIGNED NOT NULL,           -- 0..19 carrier
    item_id    SMALLINT UNSIGNED NOT NULL,
    amount     SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    durability SMALLINT UNSIGNED NOT NULL DEFAULT 100,
    PRIMARY KEY (char_id, slot),
    KEY idx_inventory_item (item_id),
    CONSTRAINT fk_inventory_char FOREIGN KEY (char_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS stash (
    char_id    INT UNSIGNED NOT NULL,
    slot       TINYINT UNSIGNED NOT NULL,           -- 0..59
    item_id    SMALLINT UNSIGNED NOT NULL,
    amount     SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    durability SMALLINT UNSIGNED NOT NULL DEFAULT 100,
    PRIMARY KEY (char_id, slot),
    CONSTRAINT fk_stash_char FOREIGN KEY (char_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS weapons (
    id         INT UNSIGNED NOT NULL AUTO_INCREMENT,
    char_id    INT UNSIGNED NOT NULL,
    weapon_id  TINYINT UNSIGNED NOT NULL,           -- GTA weapon id
    ammo       SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    weapon_condition TINYINT UNSIGNED NOT NULL DEFAULT 100,
    upgrades   INT UNSIGNED NOT NULL DEFAULT 0,     -- bitmask (suppressor, scope, ...)
    PRIMARY KEY (id),
    KEY idx_weapons_char (char_id),
    CONSTRAINT fk_weapons_char FOREIGN KEY (char_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS vehicles (
    id            INT UNSIGNED NOT NULL AUTO_INCREMENT,
    owner_char_id INT UNSIGNED NOT NULL,
    model         SMALLINT UNSIGNED NOT NULL,
    plate         VARCHAR(16) NOT NULL,
    pos_x         FLOAT NOT NULL,
    pos_y         FLOAT NOT NULL,
    pos_z         FLOAT NOT NULL,
    pos_a         FLOAT NOT NULL DEFAULT 0.0,
    color1        TINYINT UNSIGNED NOT NULL DEFAULT 1,
    color2        TINYINT UNSIGNED NOT NULL DEFAULT 1,
    paintjob      TINYINT NOT NULL DEFAULT 3,       -- 3 = none
    mods          VARCHAR(96) NOT NULL DEFAULT '',  -- "slot:comp,slot:comp"
    fuel          FLOAT NOT NULL DEFAULT 100.0,
    health        FLOAT NOT NULL DEFAULT 1000.0,
    mileage_km    FLOAT NOT NULL DEFAULT 0.0,
    is_locked     TINYINT(1) NOT NULL DEFAULT 1,
    is_impounded  TINYINT(1) NOT NULL DEFAULT 0,
    garage_zone   TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (id),
    KEY idx_vehicles_owner (owner_char_id),
    CONSTRAINT fk_vehicles_owner FOREIGN KEY (owner_char_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS bases (
    id          INT UNSIGNED NOT NULL AUTO_INCREMENT,
    char_id     INT UNSIGNED NOT NULL,
    shelter_id  TINYINT UNSIGNED NOT NULL,          -- 1..5 shelter di dead zone
    name        VARCHAR(32) NOT NULL DEFAULT 'Shelter',
    PRIMARY KEY (id),
    UNIQUE KEY uq_bases_shelter (shelter_id),
    KEY idx_bases_char (char_id),
    CONSTRAINT fk_bases_char FOREIGN KEY (char_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS barricades (
    id          INT UNSIGNED NOT NULL AUTO_INCREMENT,
    base_id     INT UNSIGNED NOT NULL,
    model       SMALLINT UNSIGNED NOT NULL,
    pos_x       FLOAT NOT NULL,
    pos_y       FLOAT NOT NULL,
    pos_z       FLOAT NOT NULL,
    rot_x       FLOAT NOT NULL DEFAULT 0.0,
    rot_y       FLOAT NOT NULL DEFAULT 0.0,
    rot_z       FLOAT NOT NULL DEFAULT 0.0,
    health      SMALLINT UNSIGNED NOT NULL DEFAULT 1500,
    PRIMARY KEY (id),
    KEY idx_barricades_base (base_id),
    CONSTRAINT fk_barricades_base FOREIGN KEY (base_id) REFERENCES bases (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS loot_nodes (
    id          INT UNSIGNED NOT NULL AUTO_INCREMENT,
    zone_id     TINYINT UNSIGNED NOT NULL,
    tier        TINYINT UNSIGNED NOT NULL,          -- 1..3
    node_kind   TINYINT UNSIGNED NOT NULL,          -- 1 rumah, 2 toko, 3 militer, 4 resource
    pos_x       FLOAT NOT NULL,
    pos_y       FLOAT NOT NULL,
    pos_z       FLOAT NOT NULL,
    respawn_at  BIGINT NOT NULL DEFAULT 0,
    is_active   TINYINT(1) NOT NULL DEFAULT 1,
    PRIMARY KEY (id),
    KEY idx_loot_zone_active (zone_id, is_active, respawn_at),
    KEY idx_loot_tier (tier)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS item_ledger (
    id         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    char_id    INT UNSIGNED NOT NULL,
    item_id    SMALLINT UNSIGNED NOT NULL,
    delta      SMALLINT NOT NULL,                   -- + masuk, - keluar
    reason     VARCHAR(32) NOT NULL,                -- loot, craft, trade, death, admin
    ref_id     INT UNSIGNED NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    PRIMARY KEY (id),
    KEY idx_ledger_char_time (char_id, created_at),
    KEY idx_ledger_item (item_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS kill_log (
    id           BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    killer_id    INT UNSIGNED NOT NULL DEFAULT 0,   -- 0 = zombie/env
    victim_id    INT UNSIGNED NOT NULL DEFAULT 0,
    victim_kind  TINYINT UNSIGNED NOT NULL,         -- 1 player, 2 zombie, 3 vehicle
    zombie_type  TINYINT UNSIGNED NOT NULL DEFAULT 0,
    weapon_id    TINYINT UNSIGNED NOT NULL,
    dist_m       FLOAT NOT NULL DEFAULT 0.0,
    zone_id      TINYINT UNSIGNED NOT NULL,
    created_at   BIGINT NOT NULL,
    PRIMARY KEY (id),
    KEY idx_kill_time (created_at),
    KEY idx_kill_killer (killer_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS anticheat_flags (
    id         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    char_id    INT UNSIGNED NOT NULL,
    flag_type  VARCHAR(32) NOT NULL,                -- TELEPORT, DPS_SPIKE, LOS_FAIL, ...
    payload    VARCHAR(255) NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL,
    PRIMARY KEY (id),
    KEY idx_flags_char_time (char_id, created_at),
    KEY idx_flags_type (flag_type, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS bans (
    id         INT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id INT UNSIGNED NOT NULL,
    reason     VARCHAR(128) NOT NULL,
    admin_name VARCHAR(24) NOT NULL DEFAULT 'console',
    expires_at BIGINT NOT NULL DEFAULT 0,           -- 0 = permanen
    created_at BIGINT NOT NULL,
    PRIMARY KEY (id),
    KEY idx_bans_account (account_id),
    CONSTRAINT fk_bans_account FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('001_init', 0);
