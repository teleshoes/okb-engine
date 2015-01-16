#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os, re
import getopt

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import predict

learn = False
lang = "en"
db_file = None
all = False

opts, args =  getopt.getopt(sys.argv[1:], 'sl:d:a')
for o, a in opts:
    if o == "-l":
        lang = a
    elif o == "-s":
        learn = True
    elif o == "-d":
        db_file = a
    elif o == "-a":
        all = True
    else:
        print("Bad option: %s" % o)
        exit(1)

p = predict.Predict()
if not db_file: db_file = os.path.join(mydir, "../db/predict-%s.db" % lang)

p.set_dbfile(db_file)
p.load_db()

def run(words):
    global p

    words = list(words)  # copy

    print("==> Words:", words)

    last_word = words.pop(-1)

    tmp = ' '.join(words)
    p.update_surrounding(tmp, len(tmp))
    p.update_preedit("")
    p._update_last_words()
    print("Context:", p.last_words)

    score, details = p._get_all_predict_scores([last_word])[last_word]

    print("Score:", score, details)

    if "clusters" in details:
        for word, cluster in reversed(list(zip(reversed(words + [last_word]), details["clusters"]))):
            print(word, p.db.get_word_by_id(cluster))

    if learn:
        p._learn(True, last_word, p.last_words)

    print()

if args:
    line = ' '.join(args)
    words = re.split(r'[^\w\-\']+', line)
    words = [ x for x in words if len(x) > 0 ]

    if all:
        words2 = []
        for w in words:
            words2.append(w)
            run(words2)
    else:
        run(words)

else:
    print("Reading text from stdin ...")
    words = []
    for line in sys.stdin.readlines():
        line = line.strip()
        if not line:
            words = []
            continue

        while True:
            mo = re.match(r'^[^\w\'\-]+', line)
            if mo:
                if re.match(r'[\.:;\!\?]', mo.group(0)): words = []  # next sentence
                line = line[mo.end(0):]

            if not line: break

            mo = re.match(r'^[\w\'\-]+', line)
            word = mo.group(0)
            line = line[mo.end(0):]

            words.append(word)
            run(words)

if learn:
    p._commit_learn(commit_all = True)
