-- Kendaraan milik pemain. Kendaraan hanya ada di dunia saat dikeluarkan dari garasi;
-- saat disimpan, bahan bakar dan kondisi ikut tersimpan.
CREATE TABLE IF NOT EXISTS owned_vehicles (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    char_id     INT UNSIGNED NOT NULL,
    model       SMALLINT UNSIGNED NOT NULL,
    plate       VARCHAR(16) NOT NULL DEFAULT '',
    fuel        FLOAT NOT NULL DEFAULT 45.0,
    health      FLOAT NOT NULL DEFAULT 1000.0,
    locked      TINYINT UNSIGNED NOT NULL DEFAULT 1,
    created_at  INT NOT NULL,
    KEY idx_owned_char (char_id),
    CONSTRAINT fk_owned_char FOREIGN KEY (char_id) REFERENCES characters(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('010_garage', UNIX_TIMESTAMP());
