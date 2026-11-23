-- 025: Riwayat peti party (ROADMAP 414).
-- "Siapa mengambil apa" harus bisa dijawab; sebelumnya hanya ada Log_Format di
-- file yang hilang setelah rotasi. stash_char_id = pemilik peti, actor = pelaku.
CREATE TABLE IF NOT EXISTS party_stash_log (
    id            BIGINT UNSIGNED  NOT NULL AUTO_INCREMENT,
    stash_char_id INT UNSIGNED     NOT NULL,
    actor_char_id INT UNSIGNED     NOT NULL,
    actor_name    VARCHAR(24)      NOT NULL DEFAULT '',
    action        VARCHAR(8)       NOT NULL,      -- take | put
    item_id       SMALLINT UNSIGNED NOT NULL,
    amount        SMALLINT UNSIGNED NOT NULL,
    created_at    BIGINT           NOT NULL,
    PRIMARY KEY (id),
    KEY idx_stash_time (stash_char_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_migrations (version, applied_at)
VALUES ('025_party_stash_log', UNIX_TIMESTAMP());
