-- /stats melakukan GROUP BY zone_id pada kill_log. Indeks lain (killer_id, created_at,
-- item_ledger, anticheat_flags) sudah dibuat di migrasi sebelumnya, jadi jangan diulang.
CREATE INDEX idx_killlog_zone ON kill_log (zone_id, created_at);

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('006_telemetry_indexes', UNIX_TIMESTAMP());
