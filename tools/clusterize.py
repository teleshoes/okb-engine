#! /usr/bin/python3
# -*- coding: utf-8 -*-

"""
Create a CSV ngram file with embedded clusters information

Typical usage :
cat corpus.txt | import_corpus <word dictionary> | clusterize.py [<options>] <cluster file> | load_sqlite <db file>
"""

import sys, os, re

import getopt

opts, args =  getopt.getopt(sys.argv[1:], 'c:w:l:r:t:')

if len(args) < 1:
    print("Usage: ", os.path.basename(__file__), " [<options>] <cluster file>")
    print(" -c <count> : max cluster n-grams ('0' means unlimited)")
    print(" -w <count> : max word n-grams ('0' means unlimited)")
    print(" -l <len>   : max cluster name length (relates to binary tree depth")
    print(" -r <file>  : output report file")
    print(" -t <file>  : threshold file")
    print(" CSV data is read from stdin, clusterized data sent to stdout")
    exit(255)

clusterfile = args[0]

rpt_file = thr_file = None
max_word = max_cluster = max_depth = None
for o, a in opts:
    if o == "-w":
        max_word = int(a)
    elif o == "-c":
        max_cluster = int(a)
    elif o == "-l":
        max_depth = int(a)
    elif o == "-r":
        rpt_file = a
    elif o == "-t":
        thr_file = a
    else:
        print("Bad option: %s" % o)
        exit(1)


# 0) Read threshold file
threshold = dict()
if thr_file:
    for li in open(thr_file, 'r').readlines():
        li = li.strip()
        if not li: continue
        key, value = re.split('\s+', li, 1)
        threshold[key] = float(value)


# 1) Read cluster file
word2cluster = dict()
with open(clusterfile, "r") as f:
    for line in f.readlines():
        line = line.strip()
        if not line: continue
        (cluster, word) = (line.split(';'))[0:2]
        if max_depth: cluster = cluster[0:max_depth]
        if cluster[0] != '@': cluster = '@' + cluster
        word2cluster[word] = cluster


# 2) import corpus as CSV
word2cluster['#NA'] = '#NA'
word2cluster['#START'] = '#START'
word2cluster['#TOTAL'] = '#CTOTAL'

grams = dict()

stats = dict()

def update_stats(ngram, cluster, count, sign, test = False):
    global grams

    if ngram.endswith("TOTAL"): return
    if ngram.startswith("#NA;#START;") or not ngram.startswith("#NA;"):
        key = "3"
    elif ngram.startswith("#NA;") and not ngram.startswith("#NA;#NA;"):
        key = "2"
    elif ngram.startswith("#NA;#NA;") and not cluster:
        key = "1"
    else: return

    key += "." + ("cluster" if cluster else "word")
    if test:
        if sign != 1: raise Exception("case not handled")
        key = "TEST." + key
        for id in [ "count", "coverage" ]:
            k = key + "." + id
            if k not in stats: stats[k] = [ 0, 0 ]
            value = count if id == "coverage" else 1
            if ngram in grams: stats[k][0] += value
            stats[k][1] += value

    else:
        key += "." + ("total" if sign > 0 else "removed")

        for id in [ "count", "coverage" ]:
            if id == "count" and grams[ngram][0] > count: continue  # already counted
            k = key + "." + id
            if k not in stats: stats[k] = 0
            value = count if id == "coverage" else 1
            stats[k] += value

def rpt_stats():
    global threshold, stats

    fail = False
    out = ""
    for size in [ 1, 2, 3 ]:
        for type in [ "word", "cluster" ]:
            for metric in [ "count", "coverage" ]:
                total = stats.get("%d.%s.total.%s" % (size, type, metric), 0)
                if not total: continue
                removed = stats.get("%d.%s.removed.%s" % (size, type, metric), 0)
                out += ("N=%d type=%s metric=%s %d/%d %.2f%%\n" %
                        (size, type, metric, total - removed, total, 100. * (total - removed) / total))

    for size in [ 1, 2, 3 ]:
        for type in [ "word", "cluster" ]:
            for metric in [ "count", "coverage" ]:
                key = "TEST.%d.%s.%s" % (size, type, metric)
                matched, total = stats.get(key, [ 0, 0 ])
                if not total: continue

                thr_info = ""
                if key in threshold:
                    thr_info = " Threshold=%.2f%%" % threshold[key]
                    if 100. * matched / total < threshold[key]:
                        thr_info += " *FAIL*"
                        fail = True
                out += ("TEST N=%d type=%s metric=%s %d/%d %.2f%%%s\n" %
                        (size, type, metric, matched, total, 100. * matched / total, thr_info))

    sys.stderr.write(out)
    if rpt_file:
        with open(rpt_file, "w") as f: f.write(out)

    if fail:
        raise Exception("Low quality corpus does not match assumptions used by language model ... Try with a larger corpus")

def add_gram(l, count, cluster = False, test = False):
    global grams

    key = ';'.join(l)

    if not test:
        if key not in grams:
            grams[key] = [ count, cluster ]
        else:
            grams[key][0] += count

    update_stats(key, cluster, count, 1, test)

def truncate():
    global grams, max_cluster, max_word

    for cluster_flag in [ False, True ]:
        max = max_cluster if cluster_flag else max_word

        if max:
            gramlist = sorted([ x for x in grams.keys() if grams[x][1] == cluster_flag],
                              key = lambda x: grams[x][0], reverse = True)

            if len(gramlist) > max:
                for key in gramlist[max:]:
                    if key.startswith("#NA;#NA;"): continue
                    update_stats(key, cluster_flag, grams[key][0], -1)
                    del grams[key]

        if max: gramlist = gramlist[:max]

n = 0
test_corpus = False
for line in sys.stdin:
    line = line.strip()

    if not line: continue

    if line.startswith('='):
        truncate()
        test_corpus = True
        continue

    (count, w1, w2, w3) = line.split(';')
    count = int(count)
    key = "%s;%s;%s" % (w1, w2, w3)

    if w3 == '#TOTAL': continue  # total n-grams are recomputed below

    if w2 == '#START':
        # first words n-grams are now considered as 2-grams [#NA, #START, word] (changed again)
        w1 = '#NA'

    add_gram([w1, w2, w3], count, False, test_corpus)
    add_gram([w1, w2, '#TOTAL'], count, False, test_corpus)

    def cluster(w):
        if w not in word2cluster: word2cluster[w] = "#NOCLUSTER"
        return word2cluster[w]

    c1, c2, c3 = cluster(w1), cluster(w2), cluster(w3)
    if "#NOCLUSTER" not in (c1, c2, c3) or test_corpus:
        add_gram([c1, c2, c3], count, True, test_corpus)
        add_gram([c1, c2, '#CTOTAL'], count, True, test_corpus)  # use separate total n-grams for words and clusters

    n += 1
    if n % 500000 == 0:
        if not test_corpus: truncate()
        sys.stderr.write("%d %d %s\n" % (n, len(grams), "(test)" if test_corpus else ""))

# 3) dump clusterized n-grams
for word, cluster in word2cluster.items():
    if not word.startswith('#'):
        print("CLUSTER;%s;%s" % (cluster, word))

truncate()

for key, gram in grams.items():
    print("%d;%s" % (gram[0], key))

rpt_stats()
