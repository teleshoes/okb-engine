#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os
import getopt

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import predict

lang = "en"
db_file = None

opts, args =  getopt.getopt(sys.argv[1:], 'l:d:')
for o, a in opts:
    if o == "-l":
        lang = a
    elif o == "-d":
        db_file = a
    else:
        print("Bad option: %s" % o)
        exit(1)

words = args

p = predict.Predict()
if not db_file: db_file = os.path.join(mydir, "../db/predict-%s.db" % lang)

p.set_dbfile(db_file)
p.load_db()

for first in [ False, True ]:
    print("== first: %s ==" % first)
    result = p.db.get_words(words, lower_first_words = set(words) if first else None)
    for word, info in result.items():
        print(word, info.id, info.cluster_id, info.real_word)
    print()
    

