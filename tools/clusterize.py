#! /usr/bin/python3
# -*- coding: utf-8 -*-

"""
Create a CSV ngram file with embedded clusters information

Typical usage :
cat corpus.txt | import_corpus <word dictionary> | clusterize.py <cluster file> | load_sqlite <db file>
"""

import sys, os

import getopt

opts, args =  getopt.getopt(sys.argv[1:], 'c:w:')

if len(args) < 1:
    print("Usage: ", os.path.basename(__file__), " [-c <max cluster n-grams>] [-w <max word n-grams>] <cluster file>")
    print(" CSV data is read from stdin, clusterized data sent to stdout")
    exit(255)

clusterfile = args[0]

max_word = max_cluster = False
for o, a in opts:
    if o == "-c":
        max_word = int(a)
    elif o == "-w":
        max_cluster = int(a)
    else:
        print("Bad option: %s" % o)
        exit(1)


# 1) Read cluster file
word2cluster = dict()
with open(clusterfile, "r") as f:
    for line in f.readlines():
        line = line.strip()
        if not line: continue
        (cluster, word) = (line.split(';'))[0:2]
        if cluster[0] != '@': cluster = '@' + cluster
        word2cluster[word] = cluster


# 2) import corpus as CSV
words = dict()
words['#TOTAL'] = -2
words['#NA'] = -1
words['#START'] = -3

for w in ['#TOTAL', '#NA', '#START']:
    word2cluster[w] = w

grams = dict()

def add_gram(l, count, cluster = False):
    global grams

    key = ';'.join(l)
    if key not in grams:
        grams[key] = [ count, cluster ]
    else:
        grams[key][0] += count

def truncate():
    global grams, max_cluster, max_word

    for cluster_flag in [ False, True ]:
        max = max_cluster if cluster_flag else max_word

        if max:
            gramlist = sorted([ x for x in grams.keys() if grams[x][1] == cluster_flag],
                              key = lambda x: grams[x][0], reverse = True)

            if len(gramlist) > max:
                for key in gramlist[max:]: del grams[key]

        if max: gramlist = gramlist[:max]

n = 0
for line in sys.stdin.readlines():
    line = line.strip()
    (count, w1, w2, w3) = line.split(';')
    count = int(count)
    key = "%s;%s;%s" % (w1, w2, w3)

    if w3 == '#TOTAL': continue  # total n-grams are recomputed below

    if w2 == '#START':
        # first words n-grams are now considered as 3-grams [#START, #START, word]
        w1 = '#START'

    add_gram([w1, w2, w3], count)
    add_gram([w1, w2, '#TOTAL'], count)

    c1, c2, c3 = word2cluster.get(w1, None), word2cluster.get(w2, None), word2cluster.get(w3, None)
    if c1 is not None and c2 is not None and c3 is not None:
        add_gram([c1, c2, c3], count)
        add_gram([c1, c2, '#TOTAL'], count)

    n += 1
    if n % 500000 == 0:
        sys.stderr.write("%d\n" % n)
        truncate()

# 3) dump clusterized n-grams
for word, cluster in word2cluster.items():
    print("CLUSTER;%s;%s" % (cluster, word))

truncate()

for key, gram in grams.items():
    print("%d;%s" % (gram[0], key))
