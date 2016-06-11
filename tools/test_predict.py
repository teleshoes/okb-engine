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
lm = language_model.LanguageModel(db, debug = True, dummy = False)

def run(words):
    global p

    words = list(words)  # copy

    print("==> Words:", words)

    last_word_choices = words.pop(-1).split('+')  # allow wordA+wordB for last word

    all_details = dict()
    result = lm.guess(words, dict([ (w, 1) for w in last_word_choices ]), verbose = True, expected_test = last_word_choices[0],
                      details_out = all_details)

    print("Result: %s/%s" % (result[0], last_word_choices[0]))

    for word in last_word_choices:
        details = all_details[word]
        score = details["score"]
        print("%15s :" % word, score, re.sub(r'(\d\.0*\d\d)\d+', lambda m: m.group(1), str(details)))

    if learn:
        lm.learn(True, last_word_choices[0], list(reversed(words)))  # learn first word if multiple choices
        # @todo handle replace

    print()

if args:
    line = ' '.join(args)
    words = re.split(r'[^\w\-\'\+]+', line)
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

if learn or replace:
    print("Learn history:", lm._learn_history)
    lm.close()
    db.close()
