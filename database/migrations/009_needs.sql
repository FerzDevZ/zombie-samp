-- Kebutuhan tubuh: lapar, haus, kandung kemih.
-- hunger/thirst sudah ada di 001 (TINYINT, belum dipakai); diubah ke FLOAT agar
-- penurunan bertahap tidak hilang karena pembulatan. bladder baru.
ALTER TABLE characters
    MODIFY hunger FLOAT NOT NULL DEFAULT 100.0,
    MODIFY thirst FLOAT NOT NULL DEFAULT 100.0,
    ADD COLUMN bladder FLOAT NOT NULL DEFAULT 0.0;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('009_needs', UNIX_TIMESTAMP());
