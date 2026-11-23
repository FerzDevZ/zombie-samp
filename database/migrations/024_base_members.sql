-- 024: Base milik party (ROADMAP 222).
-- Sebelumnya `bases.char_id` hanya mengenal satu pemilik: anggota lain tidak
-- bisa memakai gate, membangun, atau memperbaiki barricade. Tabel ini menyimpan
-- keanggotaan per karakter dengan peringkat. `bases.char_id` tetap pemilik
-- (role 2), jadi data lama tidak perlu diisi ulang.
CREATE TABLE IF NOT EXISTS base_members (
    base_id  INT UNSIGNED     NOT NULL,
    char_id  INT UNSIGNED     NOT NULL,
    role     TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- 0 member, 1 officer, 2 owner
    added_by INT UNSIGNED     NOT NULL DEFAULT 0,
    added_at BIGINT           NOT NULL,
    PRIMARY KEY (base_id, char_id),
    KEY idx_char (char_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_migrations (version, applied_at)
VALUES ('024_base_members', UNIX_TIMESTAMP());
