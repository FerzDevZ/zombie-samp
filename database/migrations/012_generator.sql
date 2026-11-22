-- Generator base: bahan bakar liter, hanya hidup saat malam.
ALTER TABLE base_structures
    ADD COLUMN fuel FLOAT NOT NULL DEFAULT 0.0 AFTER health;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('012_generator', UNIX_TIMESTAMP());
