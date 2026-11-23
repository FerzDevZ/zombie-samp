-- 023: Arsip leaderboard musiman (ROADMAP 233).
-- Leaderboard "musim" berjalan (leaderboard.inc) hanya membaca data bulan
-- berjalan; tanpa arsip, peringkat bulan lalu hilang begitu bulan berganti.
CREATE TABLE IF NOT EXISTS season_archive (
    season      VARCHAR(7)       NOT NULL,   -- 'YYYY-MM'
    category    VARCHAR(12)      NOT NULL,   -- kill | uang | jam | rank
    rank_no     TINYINT UNSIGNED NOT NULL,
    char_id     INT UNSIGNED     NOT NULL,
    char_name   VARCHAR(24)      NOT NULL,
    score       BIGINT           NOT NULL,
    archived_at BIGINT           NOT NULL,
    PRIMARY KEY (season, category, rank_no),
    KEY idx_char_season (char_id, season)
-- Collation WAJIB sama dengan `characters.name` (utf8mb4_unicode_ci): nama
-- karakter ikut di-UNION ALL dengan kolom itu di leaderboard, dan collation
-- berbeda membuat MariaDB menolak seluruh query (error 1271).
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_migrations (version, applied_at)
VALUES ('023_season_archive', UNIX_TIMESTAMP());
