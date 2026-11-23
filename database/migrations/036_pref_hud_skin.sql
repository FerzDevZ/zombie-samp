-- 036: design (skin) HUD pilihan pemain.
-- 0 = auto (klien mobile -> NEON, PC -> KACA), 1 KACA, 2 NEON, 3 TACTICAL, 4 TERANG.
ALTER TABLE characters
    ADD COLUMN IF NOT EXISTS pref_hud_skin TINYINT UNSIGNED NOT NULL DEFAULT 0
    COMMENT '0 auto, 1 kaca, 2 neon, 3 tactical, 4 terang';
