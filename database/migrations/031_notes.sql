-- Catatan kertas & papan pengumuman (fitur #34).
-- server_id di depan setiap kunci unik (invariant multi-instance).
CREATE TABLE IF NOT EXISTS notes (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    server_id TINYINT UNSIGNED NOT NULL,
    board_id TINYINT UNSIGNED NOT NULL,
    char_id INT UNSIGNED NOT NULL,
    author VARCHAR(24) NOT NULL,
    body VARCHAR(128) NOT NULL,
    created_at INT UNSIGNED NOT NULL,
    PRIMARY KEY (id),
    KEY idx_notes_board (server_id, board_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('031_notes', UNIX_TIMESTAMP());
