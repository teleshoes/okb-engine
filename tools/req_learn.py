#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" query to get learning information from predict DB """

query = "select w1.word, w2.word, w3.word, g.stock_count, g.user_count, g.user_replace, g.last_time from words w1, words w2, words w3, grams g where g.id1 = w1.id and g.id2 = w2.id and g.id3 = w3.id and user_count > 0 order by w1.word, w2.word, w3.word"

W_SIZE = 15

import sqlite3
import getopt
import sys
import time

opts, args =  getopt.getopt(sys.argv[1:], 'r')
raw = False
for o, a in opts:
    if o == "-r":
        raw = True
    else:
        print("Bad option: %s", o)
        exit(1)

if len(args) < 1:
    print("usage: ", os.path.basename(__file__), " <predict db file>")
    exit(1)

conn = sqlite3.connect(args[0])
curs = conn.cursor()
curs.execute(query)

lines = dict()
for row in curs.fetchall():
    key = ':'.join(row[0:3])
    lines[key] = row

current_day = int(time.time() / 86400)

for row in lines.values():
    (w1, w2, w3, stock_count, user_count, user_replace, last_time) = row
    if w1 == '#TOTAL': continue

    total_key = ':'.join([ '#TOTAL', w2, w3 ])
    if total_key in lines:
        total_stock_count = lines[total_key][3]
        total_user_count = lines[total_key][4]
    else:
        total_stock_count = total_user_count = -1

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
