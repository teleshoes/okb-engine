#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os, re
import getopt

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import language_model, backend

learn = replace = False
lang = "en"
db_file = None
all = False

opts, args =  getopt.getopt(sys.argv[1:], 'sl:d:ar')
for o, a in opts:
    if o == "-l":
        lang = a
    elif o == "-s":
        learn = True
    elif o == '-r':
        replace = learn = True
    elif o == "-d":
        db_file = a
    elif o == "-a":
        all = True
    else:
        print("Bad option: %s" % o)
        exit(1)

if not db_file: db_file = os.path.join(mydir, "../db/predict-%s.db" % lang)

db = backend.FslmCdbBackend(db_file)
lm = language_model.LanguageModel(db)

def run(lm, words):
    words = list(words)  # copy

    print("==> Words:", words)

    last_word_choices = words.pop(-1).split('+')  # allow wordA+wordB for last word

    print("Context:", words)

    details = dict()
    result = lm.guess(words, dict([ (x, 0) for x in last_word_choices ]),
                      verbose = True, expected_test = last_word_choices[0], details_out = details)

    clusters = dict()
    for word in last_word_choices:
        if word not in details: continue
        stats = details[word]["stats"]
        for wi in stats:
            clusters[wi.word] = db.get_cluster_by_id(wi.cluster_id)
    for w, c in clusters.items():
        print("Clusters: %15s : %s" % (w[:15], c))

    if result:
        print("Result: [%s]" % result[0], ' '.join(result[1:]))

    #@TODO learn
    # if replace:
    #     if len(last_word_choices) < 2: raise Exception("You need at leart 2 words choices (use '+' separator)")
    #     p.replace_word(last_word_choices[1], last_word_choices[0], p.last_words)
    # elif learn:
    #     p._learn(True, last_word_choices[0], p.last_words)  # learn first word if multiple choices

    print()

if args:
    line = ' '.join(args)
    words = re.split(r'[^\w\-\'\+]+', line)
    words = [ x for x in words if len(x) > 0 ]

    if all:
        words2 = [ '#START' ]
        for w in words:
            words2.append(w)
            run(lm, words2)
    else:
        run(lm, [ '#START' ] + words)

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
                if re.match(r'[\.:;\!\?]', mo.group(0)): words = [ '#START' ]  # next sentence
                line = line[mo.end(0):]

            if not line: break

            mo = re.match(r'^[\w\'\-]+', line)
            word = mo.group(0)
            line = line[mo.end(0):]

            words.append(word)
            run(words)

#@TODO
# if learn or replace:
#     print("Learn history:", p.learn_history)
#     p.commit_learn(commit_all = True)
#     p.save_db()
