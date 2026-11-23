-- Modifikasi kendaraan: zirah (pelat/bumper duri) + isi bagasi.
-- Bagasi disimpan sebagai teks "itemid:amount:durability;..." (maks 8 slot).
ALTER TABLE owned_vehicles
    ADD COLUMN plating TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER locked,
    ADD COLUMN trunk TEXT NULL AFTER plating;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('030_vehicle_mods', UNIX_TIMESTAMP());
