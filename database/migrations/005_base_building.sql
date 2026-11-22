-- Base Building: plot pribadi per karakter + struktur yang bisa dibangun, punya HP,
-- dan bisa dihancurkan zombie. Semua baris anak ikut terhapus lewat CASCADE.

ALTER TABLE bases ADD COLUMN tier TINYINT UNSIGNED NOT NULL DEFAULT 1;
ALTER TABLE bases ADD COLUMN pos_x FLOAT NOT NULL DEFAULT 0.0;
ALTER TABLE bases ADD COLUMN pos_y FLOAT NOT NULL DEFAULT 0.0;
ALTER TABLE bases ADD COLUMN pos_z FLOAT NOT NULL DEFAULT 0.0;

CREATE TABLE IF NOT EXISTS base_structures (
    id         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    base_id    INT UNSIGNED NOT NULL,
    struct_id  TINYINT UNSIGNED NOT NULL,
    pos_x      FLOAT NOT NULL,
    pos_y      FLOAT NOT NULL,
    pos_z      FLOAT NOT NULL,
    rot_z      FLOAT NOT NULL DEFAULT 0.0,
    health     INT UNSIGNED NOT NULL DEFAULT 1,
    created_at BIGINT NOT NULL,
    PRIMARY KEY (id),
    KEY idx_structures_base (base_id),
    CONSTRAINT fk_structures_base FOREIGN KEY (base_id) REFERENCES bases (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO schema_migrations (version, applied_at) VALUES ('005_base_building', UNIX_TIMESTAMP());
