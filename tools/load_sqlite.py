#! /usr/bin/python3
# -*- coding: utf-8 -*-

"""
create a sqlite database for on device usage
reads the output of import_corpus.py script
(may be piped through clusterize.py for cluster information embedding)
"""

import sys, os
import sqlite3
import time

if len(sys.argv) < 2:
    print("Usage: ", os.path.basename(__file__), " <out sqlite db file>")
    print(" CSV data is read from stdin")
    exit(255)

dbfile = sys.argv[1]
if os.path.exists(dbfile): os.unlink(dbfile)
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
words['#TOTAL'] = [ -2, -4 ]  # #TOTAL -> #CTOTAL
words['#NA'] = [ -1, -1 ]
words['#START'] = [ -3, -3 ]
words['#CTOTAL'] = [ -4, -4 ]

word_id = 1
cluster_id = -10

def w2id(word):
    global words, word_id
    if word in words: return words[word][0]
    word_id += 1
    words[word] = [ word_id, 0 ]
    return word_id

last = start = time.time()
wordcount = dict()
for line in sys.stdin.readlines():
    line = line.strip()
    if not line: continue

    cols = line.split(';')
    if cols[0] == "CLUSTER":
        cluster = cols[1]
        word = cols[2]
        if word[0] == '#': continue
        if word in words: raise Exception("Duplicate word: %s" % word)
        cid = None
        if cluster in words: cid = words[cluster][0]
        if not cid:
            cluster_id -= 1
            cid = cluster_id
            words[cluster] = [ cluster_id, 0 ]
        word_id += 1
        words[word] = [ word_id, cid ]

    else:
        (count, id3, id2, id1) = cols  # normal order in CSV files but reversed in DB
        count = int(count)
        if id3 != -1 and id1 != -2 and id1 != -4:  # only 3-grams (and not total)
            if id1 not in wordcount: wordcount[id1] = 0
            wordcount[id1] += count
        curs.execute('INSERT INTO grams (id1, id2, id3, stock_count) values (?, ?, ?, ?)', (w2id(id1), w2id(id2), w2id(id3), count))

# 4) dump words to database
print("Dump words to database ...")
curs = conn.cursor()
for word, info in words.items():
    id, cid = info

    # embed cluster information in their name to provide nice verbose output for debugging
    if cid == 0:
        words_in_cluster = [ w for w in words.keys() if words[w][1] == id ]  # all words in cluster
        words_in_cluster.sort(key = lambda x: wordcount.get(x, 0), reverse = True)  # sort by count
        word = "%s:%d:%d:%s" % (word, wordcount.get(word, 0), len(words_in_cluster), ','.join(words_in_cluster[:5]))

    curs.execute('INSERT INTO words (id, word, cluster_id, word_lc) values (?, ?, ?, ?)', (id, word, cid, word.lower()))

conn.commit()
conn.close()
