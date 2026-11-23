ALTER TABLE characters
    ADD COLUMN pref_td_mode TINYINT NOT NULL DEFAULT 0
    COMMENT '0 auto, 1 pc sprite, 2 mobile-safe box' AFTER pref_be_side;
