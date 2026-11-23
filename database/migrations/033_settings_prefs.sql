-- Preferensi /settings tahap 2: skala, kontras, efek darah, petunjuk,
-- bahasa, sisi panel bangun mobile.
ALTER TABLE characters
    ADD COLUMN pref_hud_scale TINYINT UNSIGNED NOT NULL DEFAULT 2 AFTER pref_hud_detail,
    ADD COLUMN pref_contrast TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER pref_hud_scale,
    ADD COLUMN pref_blood_fx TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER pref_contrast,
    ADD COLUMN pref_prompt TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER pref_blood_fx,
    ADD COLUMN pref_lang TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER pref_prompt,
    ADD COLUMN pref_be_side TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER pref_lang;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('033_settings_prefs', UNIX_TIMESTAMP());
