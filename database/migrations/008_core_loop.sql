-- 008: core loop baru (mayat pemain + kontrak harian)

CREATE TABLE IF NOT EXISTS graves (
    id          INT UNSIGNED NOT NULL,
    char_id     INT UNSIGNED NOT NULL,
    owner_name  VARCHAR(24) NOT NULL,
    lobby_id    TINYINT UNSIGNED NOT NULL DEFAULT 0,
    world_id    INT NOT NULL DEFAULT 0,
    pos_x       FLOAT NOT NULL,
    pos_y       FLOAT NOT NULL,
    pos_z       FLOAT NOT NULL,
    expires_at  BIGINT NOT NULL,
    created_at  BIGINT NOT NULL,
    PRIMARY KEY (id),
    KEY idx_graves_expires (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS grave_items (
    grave_id    INT UNSIGNED NOT NULL,
    slot        TINYINT UNSIGNED NOT NULL,
    item_id     SMALLINT UNSIGNED NOT NULL,
    amount      SMALLINT UNSIGNED NOT NULL,
    durability  TINYINT UNSIGNED NOT NULL DEFAULT 100,
    PRIMARY KEY (grave_id, slot),
    CONSTRAINT fk_grave_items_grave FOREIGN KEY (grave_id)
        REFERENCES graves (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS contracts (
    id          INT UNSIGNED NOT NULL AUTO_INCREMENT,
    char_id     INT UNSIGNED NOT NULL,
    day_id      INT UNSIGNED NOT NULL,
    slot        TINYINT UNSIGNED NOT NULL,
    kind        TINYINT UNSIGNED NOT NULL,
    target      SMALLINT UNSIGNED NOT NULL,
    progress    SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    reward_cash INT UNSIGNED NOT NULL DEFAULT 0,
    reward_item SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    reward_amount SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    claimed     TINYINT UNSIGNED NOT NULL DEFAULT 0,
    created_at  BIGINT NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uniq_contract (char_id, day_id, slot),
    KEY idx_contract_day (day_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('008_core_loop', UNIX_TIMESTAMP());
