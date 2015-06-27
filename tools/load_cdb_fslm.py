#! /usr/bin/python3
# -*- coding: utf-8 -*-

"""
generate 2 database files:
 + separate compressed storage for stock counts
 + simple C struct for words and user n-grams

read the output of import_corpus.py script
(may be piped through clusterize.py for cluster information embedding)
"""

import sys, os
import time

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '../ngrams')
sys.path.insert(0, libdir)

import fslm_ngrams
import cdb

if len(sys.argv) < 2:
    print("Usage: ", os.path.basename(__file__), " <out sqlite db file>")
    print(" CSV data is read from stdin")
    exit(255)

dbfile = sys.argv[1]
if os.path.exists(dbfile): os.unlink(dbfile)

fslm_file = dbfile
if fslm_file[-3:] == '.db': fslm_file = fslm_file[:-3]
fslm_file += '.ng'

txt_file = fslm_file[:-3] + ".rpt"  # DB text version for debugging


# 2) import corpus as CSV
print("Import CSV corpus data ...")
words = dict()  # word -> [ word_id, cluster_id ]
words['#TOTAL'] = ( -2, -4 )  # #TOTAL -> #CTOTAL
words['#NA'] = ( -1, -1 )
words['#START'] = ( -3, -3 )
words['#CTOTAL'] = ( -4, -4 )

fslm_encoder = fslm_ngrams.NGramEncoder(base_bits = 4,
                                        block_size = 128,
                                        progress = True)

txtf = open(txt_file, 'w')
cur_id = 10

def w2id(word):
    global words, cur_id
    if word in words: return words[word][0]
    cur_id += 1
    words[word] = (cur_id, 0)
    return cur_id

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
        if cluster in words:
            cid = words[cluster][0]
        else:
            cur_id += 1
            cid = cur_id
            words[cluster] = (cur_id, 0)
        cur_id += 1
        words[word] = (cur_id, cid)

    else:
        (count, w1, w2, w3) = cols
        count = int(count)
        gram = [ w1, w2, w3 ]
        gram = [ w2id(w) for w in gram ]
        if w1 != '#NA' and w3 != '#TOTAL' and w3 != '#CTOTAL':  # only 3-grams (and not total)
            if w3 not in wordcount: wordcount[w3] = 0
            wordcount[w3] += count

        # remove #NA at the beginning (the store can handle variable length n-grams)
        while gram[0] == -1: gram.pop(0)
        gram = [ abs(x) for x in gram ]  # store can only handle positive identifiers

        # n-grams are sent to optimized fslm database
        # sqlite n-gram table will start as empty and will fill for learning
        fslm_encoder.add_ngram(gram, count)

        # write text version
        txtf.write('GRAM %s %s\n' % (gram, count))

# 3) write FSLM compressed DB
print("Dumping compressed ngram file ...")
fslm = fslm_encoder.get_bytes()
with open(fslm_file, 'wb') as f:
    f.write(fslm)

# 4) dump words to database
print("Dumping words to database ...")
cdb.load(dbfile)

lwords = dict()
for word, info in words.items():
    if info[1] == 0:
        # store cluster information in DB to provide nice verbose output for debugging
        words_in_cluster = [ w for w in words.keys() if words[w][1] == info[0] ]  # all words in cluster
        words_in_cluster.sort(key = lambda x: wordcount.get(x, 0), reverse = True)  # sort by count

        cluster_info = ("%s:%d:%d:%s" % (word, wordcount.get(word, 0), len(words_in_cluster),
                                         ','.join(words_in_cluster[:5])))[:50]  # <- size limitation in C module (& spare room for unicode chars)
        txtf.write("CLUSTER %d %s\n" % (info[0], cluster_info))
        cdb.set_string("cluster-%d" % info[0], cluster_info)

    else:
        lword = word.lower()
        if lword not in lwords: lwords[lword] = dict()
        lwords[lword][word] = info

for lword, words in lwords.items():
    cdb.set_words(lword, words)
    txtf.write("WORD %s %s\n" % (lword, words))

cdb.save()
cdb.clear()

txtf.close()
