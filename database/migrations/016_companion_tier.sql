-- Tier rekrutan (0 dasar, 1 rompi taktis, 2 perlengkapan penuh).
ALTER TABLE companions
    ADD COLUMN tier TINYINT NOT NULL DEFAULT 0 AFTER mode;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('016_companion_tier', UNIX_TIMESTAMP());
