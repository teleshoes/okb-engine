#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" factory reset  """

import cdb
import sys, os
import re

if len(sys.argv) < 2:
    print("usage: " + os.path.basename(__file__) + " <db file>")
    exit(1)

update = False

cdb.load(sys.argv[1])
lst = cdb.get_keys()
for key in lst:
    if key.startswith("param_"):
        continue
    elif key.startswith("cluster-"):
        continue
    elif re.match(r'^[0-9\-]+:[0-9\-]+:[0-9\-]+$', key):
        cdb.rm(key)
        update = True
    elif re.match(r'^[\w\-\']+$', key):
        words = cdb.get_words(key)
        for word, info in list(words.items()):
            if info[0] >= 1000000:
                update = True
                del words[word]
        if words:
            cdb.set_words(key, words)
        else:
            cdb.rm(key)

if update: cdb.save()
cdb.clear()
