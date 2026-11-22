-- 020: Index query panas (audit database T6/T7/T15, 26-11-19).
-- kill_log bertambah 1 baris per kill zombie; tanpa index berprefix
-- victim_kind, /top KILL & MUSIM membayar full index scan + filesort.

-- MUSIM: range created_at + covering killer_id (GROUP BY index-only).
CREATE INDEX idx_killlog_kind_time_killer ON kill_log (victim_kind, created_at, killer_id);

-- KILL all-time: index-only GROUP BY killer_id per victim_kind.
CREATE INDEX idx_killlog_kind_killer ON kill_log (victim_kind, killer_id);

-- /stats: tiga GROUP BY per killer (zone/weapon/type) menjadi index-only.
CREATE INDEX idx_killlog_killer_cover ON kill_log (killer_id, zone_id, weapon_id, zombie_type);

-- Leaderboard characters: ORDER BY cash/rank_points/playtime_sec DESC LIMIT 5.
CREATE INDEX idx_characters_cash ON characters (cash);
CREATE INDEX idx_characters_rank ON characters (rank_points);
CREATE INDEX idx_characters_playtime ON characters (playtime_sec);

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('020_hot_query_indexes', UNIX_TIMESTAMP());
