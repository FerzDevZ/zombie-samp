-- Skill tree: poin dari level up, 5 cabang, maks 3 level per cabang.
ALTER TABLE characters ADD COLUMN skill_points SMALLINT UNSIGNED NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS character_skills (
    char_id   INT UNSIGNED NOT NULL,
    skill_id  TINYINT UNSIGNED NOT NULL,
    level     TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (char_id, skill_id),
    CONSTRAINT fk_character_skills_char FOREIGN KEY (char_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('004_skills', UNIX_TIMESTAMP());
