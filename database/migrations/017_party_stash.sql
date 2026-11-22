-- Peti bersama party: tersimpan pada karakter pemimpin party.
CREATE TABLE IF NOT EXISTS party_stash (
    char_id    INT UNSIGNED NOT NULL,
    slot       TINYINT UNSIGNED NOT NULL,
    item_id    SMALLINT UNSIGNED NOT NULL,
    amount     SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    durability SMALLINT UNSIGNED NOT NULL DEFAULT 100,
    PRIMARY KEY (char_id, slot),
    CONSTRAINT fk_party_stash_char FOREIGN KEY (char_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('017_party_stash', UNIX_TIMESTAMP());
