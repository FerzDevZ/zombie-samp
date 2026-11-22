-- 014: NPC rekrutan (survivor companion) per karakter.
-- Satu karakter hanya boleh punya satu rekrutan (UNIQUE char_id), jadi
-- INSERT ... ON DUPLICATE KEY UPDATE aman dipakai sebagai jalur save.

CREATE TABLE IF NOT EXISTS companions (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    char_id INT UNSIGNED NOT NULL,
    name VARCHAR(24) NOT NULL,
    health FLOAT NOT NULL DEFAULT 100,
    mode TINYINT NOT NULL DEFAULT 0,
    recruited_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uniq_companion_char (char_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('014_companion', UNIX_TIMESTAMP());
