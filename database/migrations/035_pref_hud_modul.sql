-- 035: modul kolom HUD kendaraan bisa dinyalakan/dimatikan sendiri.
-- Satu kolom bitmask, bukan tiga kolom boolean: menambah baris HUD ke-4 nanti
-- cukup memakai bit berikutnya tanpa migrasi baru.
--   bit 1 = baris identitas mobil (MOBIL ...)
--   bit 2 = speedometer (angka + bar km/j)
--   bit 4 = baris GPS + bar progres
ALTER TABLE characters
    ADD COLUMN IF NOT EXISTS pref_hud_modul TINYINT UNSIGNED NOT NULL DEFAULT 7
    COMMENT 'bitmask kolom kendaraan: 1 info, 2 speedometer, 4 GPS';
