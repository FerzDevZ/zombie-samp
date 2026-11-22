-- Profil karakter: pilihan pembuatan awal (gender, umur, ras, asal, pekerjaan,
-- trait, penampilan). Kolom dipakai untuk identitas karakter dan ringkasan /profil.
ALTER TABLE characters
    ADD COLUMN gender     TINYINT(3) UNSIGNED NOT NULL DEFAULT 0 AFTER skin,
    ADD COLUMN age        TINYINT(3) UNSIGNED NOT NULL DEFAULT 0 AFTER gender,
    ADD COLUMN race       TINYINT(3) UNSIGNED NOT NULL DEFAULT 0 AFTER age,
    ADD COLUMN origin     TINYINT(3) UNSIGNED NOT NULL DEFAULT 0 AFTER race,
    ADD COLUMN occupation TINYINT(3) UNSIGNED NOT NULL DEFAULT 0 AFTER origin,
    ADD COLUMN trait      TINYINT(3) UNSIGNED NOT NULL DEFAULT 0 AFTER occupation,
    ADD COLUMN appearance TINYINT(3) UNSIGNED NOT NULL DEFAULT 0 AFTER trait;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('018_character_profile', UNIX_TIMESTAMP());
