-- 002: satu baris senjata per (karakter, weapon_id) supaya REPLACE ... ON DUPLICATE bekerja.
ALTER TABLE weapons ADD UNIQUE KEY uq_weapons_char_weapon (char_id, weapon_id);

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('002_weapons_unique', UNIX_TIMESTAMP());
