PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS save_file_metadata (
    save_id INTEGER PRIMARY KEY REFERENCES save_games(id) ON DELETE CASCADE,
    career_id TEXT NOT NULL UNIQUE,
    save_kind TEXT NOT NULL
        CHECK (save_kind IN ('career', 'autosave')),
    parent_career_id TEXT,
    autosave_slot INTEGER NOT NULL DEFAULT 0
        CHECK (autosave_slot BETWEEN 0 AND 3),
    play_time_seconds INTEGER NOT NULL DEFAULT 0
        CHECK (play_time_seconds >= 0),
    saved_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_save_file_metadata_parent
    ON save_file_metadata (parent_career_id, save_kind, autosave_slot);

INSERT OR IGNORE INTO schema_migrations (version) VALUES (3);
