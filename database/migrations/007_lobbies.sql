-- Sub-lobby: pemain mengingat lobby terakhirnya. World virtual tetap dipisah per lobby.
ALTER TABLE characters ADD COLUMN lobby_id TINYINT NOT NULL DEFAULT 0;
ALTER TABLE bases ADD COLUMN lobby_id TINYINT NOT NULL DEFAULT 0;
ALTER TABLE barricades ADD COLUMN lobby_id TINYINT NOT NULL DEFAULT 0;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('007_lobbies', UNIX_TIMESTAMP());
