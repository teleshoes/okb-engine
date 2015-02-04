DROP INDEX IF EXISTS words_idx;
DROP INDEX IF EXISTS words_idx2;
DROP INDEX IF EXISTS words_idx3;
DROP INDEX IF EXISTS grams_idx;
DROP TABLE IF EXISTS grams;
DROP TABLE IF EXISTS words;
DROP TABLE IF EXISTS params;
DROP TABLE IF EXISTS version;

CREATE TABLE grams (
   id1 INT NOT NULL,
   id2 INT NOT NULL,
   id3 INT NOT NULL,
   stock_count INT NOT NULL,
   user_count REAL,
   user_replace REAL,
   last_time INT
);

CREATE INDEX grams_idx ON grams (id1, id2, id3);
CREATE INDEX grams_idx2 ON grams (user_count);

CREATE TABLE words (
   id INTEGER PRIMARY KEY NOT NULL, /* we get autoincrement for free */
   word TEXT NOT NULL,
   cluster_id INT,
   word_lc TEXT
);

CREATE INDEX words_idx ON words (word);
CREATE INDEX words_idx2 ON words (id);
CREATE INDEX words_idx3 ON words (word_lc);

CREATE TABLE params (
    key TEXT NOT NULL,
    value REAL
);

CREATE TABLE version (
   version INT
);

INSERT INTO version (version) VALUES (1);
