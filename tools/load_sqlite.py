#! /usr/bin/python3
# -*- coding: utf-8 -*-

"""
create a sqlite database for on device usage
reads the output of import_corpus.py script
"""

import sys, os
import sqlite3
import time

if len(sys.argv) < 2:
    print("Usage: ", os.path.basename(__file__), " <out sqlite db file>")
    print(" CSV data is read from stdin")
    exit(255)

dbfile = sys.argv[1]
conn = sqlite3.connect(dbfile)


# 1) RAZ db
print("Init database ...")
initfile = os.path.join(os.path.dirname(__file__), "init.sql")
curs = conn.cursor()
with open(initfile, "r") as f:
    for stmt in f.read().split(';'):
        curs.execute(stmt)
conn.commit()

# 2) import corpus as CSV
print("Import CSV corpus data ...")
words = dict()
words['#TOTAL'] = -2
words['#NA'] = -1
words['#START'] = -3
cur_id = 1

def w2id(word):
    global words, cur_id
    if word in words: return words[word]
    cur_id += 1
    words[word] = cur_id
    return cur_id

line = sys.stdin.readline()
last = start = time.time()
n = 0
while line:
    line = line.strip()
    n += 1
    (count, id3, id2, id1) = line.split(';')
    curs.execute('INSERT INTO grams (id1, id2, id3, stock_count) values (?, ?, ?, ?)', (w2id(id1), w2id(id2), w2id(id3), count))

    line = sys.stdin.readline()

    now = time.time()
    if now >= last + 5:
        print("[%d] n-grams: %d - words: %d" % (int(now - start), n, len(words)))
        last = now

# 4) dump words to database
print("Dump words to database ...")
print("[%d] n-grams: %d - words: %d" % (int(now - start), n, len(words)))
curs = conn.cursor()
for word, id in words.items():
    curs.execute('INSERT INTO words (id, word) values (?, ?)', (id, word))

conn.commit()
conn.close()


# bye
print("OK")
