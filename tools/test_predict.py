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

def run(context, candidates):
    global p

    print("==> Context:", context, "Words:", candidates)

    all_details = dict()
    result = lm.guess(['#START'] + context,
                      dict([ (' '.join(c) if isinstance(c, (list, tuple)) else c, 1.) for c in candidates ]),
                      verbose = True,
                      details_out = all_details)
    print("Result: %s" % result[0])

    if learn:
        candidate = candidates[0]  # learn first word if multiple choices
        if not isinstance(candidate, (list, tuple)): candidate = [ candidate ]
        for word in candidate:
            lm.learn(True, word, list(reversed(context)))
            context = context + [ word ]
        # @todo handle replace

    print()

context = []
if args:
    line = ' '.join(args)
    words = re.split(r'[^\w\-\'\+]+', line)
    words = [ x for x in words if len(x) > 0 ]
    candidates = None

    while words:
        word = words.pop(0)
        if word.find("+") >= 0:
            if candidates: raise Exception("only one multiple choice supported (with '+')")
            candidates = [ [ x ] for x in word.split('+') ]
            continue

        if candidates:
            for c in candidates: c.append(word)

        else:
            if all or not words: run(context, [ word ])
            context.append(word)

    if candidates:
        run(context, candidates)

else:
    print("Reading text from stdin ...")
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

            run(context, [ word ])
            context.append(word)

if learn or replace:
    print("Learn history:", lm._learn_history)
    lm.close()
    db.close()
