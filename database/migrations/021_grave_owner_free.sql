-- 021: Eksklusivitas pemilik makam bertahan restart (audit F-20).
-- Sebelumnya gvOwnerFreeTick di-set = now saat boot load -> semua makam
-- langsung bebas dijarah setelah restart. owner_free_at = epoch detik.
ALTER TABLE graves ADD COLUMN owner_free_at INT UNSIGNED NOT NULL DEFAULT 0;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('021_grave_owner_free', UNIX_TIMESTAMP());
