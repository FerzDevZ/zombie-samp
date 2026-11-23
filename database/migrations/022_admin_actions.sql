-- 022: Audit log admin di tabel sendiri (ROADMAP 389/402/430).
-- Sebelumnya hanya Log_Format ke file (hilang saat rotasi) dan bans.admin_name
-- (hanya untuk ban). Tabel ini menyimpan siapa melakukan apa ke siapa.
CREATE TABLE IF NOT EXISTS admin_actions (
    id             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id     INT UNSIGNED    NOT NULL DEFAULT 0,
    admin_name     VARCHAR(24)     NOT NULL DEFAULT '',
    action         VARCHAR(32)     NOT NULL,
    target_account INT UNSIGNED    NOT NULL DEFAULT 0,
    detail         VARCHAR(180)    NOT NULL DEFAULT '',
    created_at     BIGINT          NOT NULL,
    PRIMARY KEY (id),
    KEY idx_action_time (action, created_at),
    KEY idx_account_time (account_id, created_at),
    KEY idx_target (target_account, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO schema_migrations (version, applied_at)
VALUES ('022_admin_actions', UNIX_TIMESTAMP());
