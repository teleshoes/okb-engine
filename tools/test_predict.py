#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os, re

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import predict


line = ' '.join(sys.argv[1:])
words = re.split(r'[^a-zA-Z\-\']+', line)
words = [ x for x in words if len(x) > 0 ]

print("Words:", words)

p = predict.p

last_word = words.pop(-1)

p.set_dbfile(os.path.join(mydir, "../db/predict-en.db"))
tmp = ' '.join(words)
p.update_surrounding(tmp, len(tmp))
p.update_preedit("")
p._update_last_words()

score = p._get_all_predict_scores([last_word])

print("Score:", score)
