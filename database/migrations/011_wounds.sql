-- Luka terbuka: 0..3 tumpukan, menyebabkan perdarahan tiap 5 detik.
ALTER TABLE characters
    ADD COLUMN wounds TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER bladder;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('011_wounds', UNIX_TIMESTAMP());
