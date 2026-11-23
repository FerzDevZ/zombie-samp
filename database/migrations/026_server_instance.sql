-- 026: Akun lintas server (ROADMAP 315).
-- Satu database boleh dipakai beberapa instance (s1/s2/...). Karakter dimiliki
-- oleh satu instance supaya daftar karakter di s1 tidak menampilkan karakter s2,
-- dan `/pilih <id>` tidak bisa menyeberang instance.
-- `accounts` tetap bersama: login, password, dan ban berlaku lintas instance.
ALTER TABLE characters
    ADD COLUMN IF NOT EXISTS server_id TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER id,
    ADD KEY IF NOT EXISTS idx_server_slot (server_id, account_id);

INSERT IGNORE INTO schema_migrations (version, applied_at)
VALUES ('026_server_instance', UNIX_TIMESTAMP());
