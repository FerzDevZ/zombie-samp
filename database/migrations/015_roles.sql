-- 015: level admin per akun (0 = pemain, 1 = admin, 2 = superadmin).
-- Sebelum ini gating admin memakai native RCON `IsPlayerAdmin` sehingga tidak ada
-- cara memberi admin permanen lewat database.

ALTER TABLE accounts
    ADD COLUMN admin_level TINYINT NOT NULL DEFAULT 0 AFTER is_banned,
    ADD KEY idx_accounts_admin (admin_level);
INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('015_roles', UNIX_TIMESTAMP());
