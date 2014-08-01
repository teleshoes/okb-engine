#! /usr/bin/python

# lenient result comparison tool
# (because results are a bit different in incremental mode, and unfortunately non-deterministic in threaded mode)

ignore_threshold1 = 0.8
ignore_threshold2 = 0.9
max_ratio = 0.05

import sys

def load(fname):
    result = dict()
    for line in file(fname).readlines():
        line = line.strip()
        if not line: continue
        tmp = line.split(' ')
        result[tmp[0]] = float(tmp[1])
    return result

f1 = load(sys.argv[1])
f2 = load(sys.argv[2])

max_score = 0
score = dict()
for word in set(f1.keys()).union(f2.keys()):
    t = 0
    n = 0
    if word in f1: n, t = n + 1, t + f1[word]
    if word in f2: n, t = n + 1, t + f2[word]
    score[word] = t / n
    max_score = max(score[word], max_score)

def err(text):
    print "***", text
    exit(1)

lst = sorted(score.keys(), key = lambda x: score[x], reverse = True)

while lst:
    word = lst.pop(0)

    s1 = f1.get(word, 0)
    s2 = f2.get(word, 0)

    if s1 < max_score * ignore_threshold1 and s2 < max_score * ignore_threshold2: exit(0)
    if s1 < max_score * ignore_threshold2 and s2 < max_score * ignore_threshold1: exit(0)

    if not s1 or not s2: err("missing word: [%s]" % word)

    if s1 / s2 < 1 - max_ratio or s1 / s2 > 1 + max_ratio: err("score gap: %s [%.2f] [%.2f]" % (word, s1, s2))

exit(0)
