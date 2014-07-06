DROP INDEX IF EXISTS words_idx;
DROP INDEX IF EXISTS grams_idx;
DROP TABLE IF EXISTS grams;
DROP TABLE IF EXISTS words;
DROP TABLE IF EXISTS version;

CREATE TABLE grams (
   id1 INT NOT NULL,
   id2 INT NOT NULL,
   id3 INT NOT NULL,
   stock_count INT NOT NULL,
   user_count INT,
   user_last INT
);

CREATE INDEX grams_idx ON grams (id1, id2, id3);

CREATE TABLE words (
   id INT PRIMARY KEY NOT NULL,
   word VARCHAR(50) NOT NULL
);

CREATE INDEX words_idx ON words (word);

CREATE TABLE version (
   version INT
);

INSERT INTO version (version) VALUES (1);
