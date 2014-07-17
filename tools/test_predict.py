#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os, re
import getopt

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import predict

lang = "en"
opts, args =  getopt.getopt(sys.argv[1:], 'l:')
for o, a in opts:
    if o == "-l":
        lang = a
    else:
        print("Bad option: %s", o)
        exit(1)

line = ' '.join(args)
words = re.split(r'[^\w\-\']+', line)
words = [ x for x in words if len(x) > 0 ]

print("Words:", words)

p = predict.Predict()

last_word = words.pop(-1)

p.set_dbfile(os.path.join(mydir, "../db/predict-%s.db" % lang))
p.load_db()
tmp = ' '.join(words)
p.update_surrounding(tmp, len(tmp))
p.update_preedit("")
p._update_last_words()
print("Context:", p.last_words)

score = p._get_all_predict_scores([last_word])

print("Score:", score)
