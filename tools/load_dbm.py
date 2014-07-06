#! /usr/bin/python2
# -*- coding: utf-8 -*-
#

"""
create a dbm database for on device usage
reads the output of import_corpus.py script
(never used: ndbm & gdbm support is not available on python3/dbm on Jolla)
"""

import sys, os
import dumbdbm as dbm  # ndbm & gdbm don't work on Jolla ?
import time

if len(sys.argv) < 2:
    print "Usage: ", os.path.basename(__file__), " <out sqlite db file>"
    print " CSV data is read from stdin"
    exit(255)

dbfile = sys.argv[1]
db = dbm.open(dbfile, 'n')


# 2) import corpus as CSV
print "Import CSV corpus data ..."
words = dict()
words['#TOTAL'] = -2
words['#NA'] = -1
words['#START'] = 0
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
    key = ':'.join([str(w2id(x)) for x in [ id1, id2, id3 ]])
    value = '%d' % int(count)
    db[key] = value

    line = sys.stdin.readline()

    now = time.time()
    if now >= last + 5:
        print "[%d] n-grams: %d - words: %d" % (int(now - start), n, len(words))
        last = now

# 4) dump words to database
print "Dump words to database ..."
print "[%d] n-grams: %d - words: %d" % (int(now - start), n, len(words))
for word, id in words.items():
    db[word.encode('utf-8')] = str(id)

db.close()

# bye
print "OK"
