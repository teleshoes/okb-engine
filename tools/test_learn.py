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
p.set_dbfile(os.path.join(libdir, "db/predict-%s.db" % lang))
p.load_db()


txt=""
p.update_surrounding("", 0)
for word in words:
    txt = txt + word + " "
    p.update_surrounding(txt, len(txt))
    
p._commit_learn(commit_all = True)

print("OK")

