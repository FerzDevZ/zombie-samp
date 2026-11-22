-- 013: onboarding quest 5 langkah (persisten per karakter)
ALTER TABLE characters
    ADD COLUMN tutorial_step TINYINT NOT NULL DEFAULT 0 AFTER wounds,
    ADD COLUMN tutorial_count INT NOT NULL DEFAULT 0 AFTER tutorial_step;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('013_tutorial', UNIX_TIMESTAMP());
