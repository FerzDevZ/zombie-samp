-- 027: Log kematian pemain (ROADMAP baru).
-- Sebelum ini `OnPlayerDeath` membuang killerid (`#pragma unused killerid`)
-- sehingga tidak ada cara menjawab "kenapa saya mati" dan tidak ada bukti
-- untuk mendeteksi kill farming. kill_log hanya mencatat pembunuhan zombie.
CREATE TABLE IF NOT EXISTS deaths (
    id             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    victim_char_id INT UNSIGNED    NOT NULL,
    killer_char_id INT UNSIGNED    NOT NULL DEFAULT 0,
    victim_kind    TINYINT UNSIGNED NOT NULL,   -- 0 zombie, 1 pemain, 2 lingkungan
    weapon         TINYINT UNSIGNED NOT NULL,
    pos_x          FLOAT           NOT NULL DEFAULT 0,
    pos_y          FLOAT           NOT NULL DEFAULT 0,
    pos_z          FLOAT           NOT NULL DEFAULT 0,
    created_at     BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (id),
    KEY idx_victim_time (victim_char_id, created_at),
    KEY idx_killer_time (killer_char_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_migrations (version, applied_at)
VALUES ('027_deaths', UNIX_TIMESTAMP());
