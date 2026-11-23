-- 029: Buang idx_server_slot yang kini duplikat.
--
-- 026 menambahkan KEY idx_server_slot (server_id, account_id) sebagai penopang
-- query saat itu. Setelah 028 membuat UNIQUE (server_id, account_id, slot),
-- kolom (server_id, account_id) hanyalah prefiks dari kunci unik itu — MySQL
-- bisa memakainya, jadi indeks terpisah hanya menambah biaya tulis pada tabel
-- yang paling sering di-update (cash/level/posisi karakter).
--
-- Dijalankan kondisional supaya aman pada instalasi baru maupun lama.
SET @has_index := (
    SELECT COUNT(*) FROM information_schema.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND INDEX_NAME = 'idx_server_slot'
);
SET @stmt := IF(@has_index > 0, 'ALTER TABLE characters DROP INDEX idx_server_slot', 'DO 0');
PREPARE drop_server_slot FROM @stmt;
EXECUTE drop_server_slot;
DEALLOCATE PREPARE drop_server_slot;

INSERT IGNORE INTO schema_migrations (version, applied_at)
VALUES ('029_drop_redundant_server_index', UNIX_TIMESTAMP());
