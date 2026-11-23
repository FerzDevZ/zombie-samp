-- Gabungan: modifikasi senjata (fitur #16) + preferensi /settings.
-- server_id tidak relevan di kolom characters (baris karakter milik satu
-- server via characters.server_id yang sudah ada).
ALTER TABLE characters
    ADD COLUMN watt_pistol_sil TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER rank_points,
    ADD COLUMN watt_pistol_mag TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER watt_pistol_sil,
    ADD COLUMN watt_rifle_scope TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER watt_pistol_mag,
    ADD COLUMN watt_rifle_mag TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER watt_rifle_scope,
    ADD COLUMN watt_sil_shots SMALLINT UNSIGNED NOT NULL DEFAULT 0 AFTER watt_rifle_mag,
    ADD COLUMN pref_hud_gaya TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER watt_sil_shots,
    ADD COLUMN pref_hud_detail TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER pref_hud_gaya;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('032_weapon_attach_settings', UNIX_TIMESTAMP());
