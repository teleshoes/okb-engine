#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" dump DB content (new DB format) """

import cdb
import sys, os
import re

if len(sys.argv) < 2:
    print("usage: " + os.path.basename(__file__) + " <db file>")
    exit(1)

cdb.load(sys.argv[1])
lst = cdb.get_keys()
for key in lst:
    if key.startswith("param_"):
        print("PARAM %s %s" % (key, cdb.get_string(key)))
    elif key.startswith("cluster-"):
        print("CLUSTER %s %s" % (key, cdb.get_string(key)))
    elif re.match(r'^[0-9\-]+:[0-9\-]+:[0-9\-]+$', key):
        print("NGRAM %s %s" % (key, cdb.get_gram(key)))
    elif re.match(r'^[\w\-\'\#]+$', key):
        print("WORD %s %s" % (key, cdb.get_words(key)))
    else:
        print("UNKNOWN %s" % key)
