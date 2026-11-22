-- Hotbar cepat 1-5: menyimpan 5 id item (0 = kosong) sebagai CSV.
-- Contoh: "10,0,0,26,0" berarti slot 1 = Bandage, slot 4 = Adrenaline Shot.
ALTER TABLE characters
    ADD COLUMN hotbar VARCHAR(32) NOT NULL DEFAULT '' AFTER appearance;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('019_hotbar', UNIX_TIMESTAMP());
