-- 028: Unique key karakter harus mengenal instance.
--
-- Sebelum ini `characters` memakai UNIQUE(name) dan UNIQUE(account_id, slot)
-- GLOBAL, sisa skema single-server. Akibatnya 026 (server_id) hanya memisahkan
-- baris: akun tidak bisa memakai nama yang sama di dua instance, dan tidak bisa
-- memakai slot 0 di dua instance sekaligus. Bukti: INSERT server_id=2 dengan
-- name='FerzDevZ' ditolak "Duplicate entry 'FerzDevZ' for key
-- 'uq_characters_name'".
--
-- Setelah ini kunci menjadi per instance:
--   (server_id, name)              -> nama unik di dalam satu instance
--   (server_id, account_id, slot)  -> satu akun boleh punya slot 0 di s1 dan s2
ALTER TABLE characters
    DROP INDEX uq_characters_name,
    DROP INDEX uq_characters_slot,
    ADD UNIQUE KEY uq_characters_server_name (server_id, name),
    ADD UNIQUE KEY uq_characters_server_slot (server_id, account_id, slot);

INSERT IGNORE INTO schema_migrations (version, applied_at)
VALUES ('028_character_keys_per_server', UNIX_TIMESTAMP());
