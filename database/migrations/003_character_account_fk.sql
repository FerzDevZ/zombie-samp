-- characters.account_id tidak punya foreign key: menghapus akun meninggalkan
-- karakter yatim (dan lewat karakter, base + barricade + inventory yatim).
-- Bersihkan dulu sisa data, lalu pasang constraint dengan CASCADE.

DELETE FROM characters WHERE account_id NOT IN (SELECT id FROM accounts);
DELETE FROM bases WHERE char_id NOT IN (SELECT id FROM characters);
DELETE FROM barricades WHERE base_id NOT IN (SELECT id FROM bases);

ALTER TABLE characters
    ADD CONSTRAINT fk_characters_account
    FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('003_character_account_fk', UNIX_TIMESTAMP());
