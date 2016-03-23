#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" query to get learning information from predict DB """

W_SIZE = 15

import getopt
import sys
import time
import os
import re

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import backend

opts, args =  getopt.getopt(sys.argv[1:], 'r')
raw = False
for o, a in opts:
    if o == "-r":
        raw = True
    else:
        print("Bad option: %s", o)
        exit(1)

if len(args) < 1:
    print("usage:", os.path.basename(__file__), "[-r] <predict db file>")
    exit(1)

db = backend.FslmCdbBackend(args[0])
lst = db.get_keys()

current_day = int(time.time() / 86400)

id_cache = dict()

def id2w(id):
    global id_cache, db
    if id in id_cache: return id_cache[id]

    id_cache[id] = word = db.get_word_by_id(id)  # may be null
    return word


for key in lst:
    if not re.match(r'^[0-9\-]+:[0-9\-]+:[0-9\-]+$', key): continue

    key = [ int(x) for x in key.split(':') ]
    total_key = [ -2 ] + key[1:]
    if key == total_key: continue

    gr1 = db.get_gram(key)
    gr2 = db.get_gram(total_key)
    if not gr1 and not gr2: continue

    (stock_count, user_count, user_replace, last_time) = gr1
    (total_stock_count, total_user_count, dummy1, dummy2) = gr2

    (w1, w2, w3) = [ id2w(int(id)) or "???" for id in key ]

    if raw:
        print("%s;%s;%s;%f;%f;%f;%f;%f;%d" % (w1, w1, w3,
                                              stock_count, 1.0 * stock_count / total_stock_count if total_stock_count > 0 else -1,
                                              user_count, 1.0 * user_count / total_user_count if total_user_count > 0 else -1,
                                              user_replace, last_time))
    else:
        cols = []
        fmt = "%" + str(W_SIZE) + "s"
        for w in (w3, w2, w1):
            if w == '#NA': w = '=' * W_SIZE
            cols.append(fmt % w[0:W_SIZE])

        cols.append("%7d" % stock_count)
        if total_stock_count > 0: cols.append("%8.1e" % (1.0 * stock_count / total_stock_count))
        else: cols.append("********")

        cols.append("%7.2f" % user_count)
        if total_user_count > 0: cols.append("%8.1e" % (1.0 * user_count / total_user_count))
        else: cols.append("********")

        cols.append("%7.2f" % user_replace)
        cols.append("%4d" % (current_day - last_time))

        print('|' + '|'.join(cols) + '|')
