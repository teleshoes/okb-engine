#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" dump DB content (new DB format) """

import cdb
import sys, os

if len(sys.argv) < 2:
    print("usage: " + os.path.basename(__file__) + " <db file> [<parameter> [<new value>] ]")
    exit(1)

cdb.load(sys.argv[1])
if len(sys.argv) >= 4:
    cdb.set_string("param_%s" % sys.argv[2], sys.argv[3])
    cdb.save()
else:
    if len(sys.argv) >= 3:
        keys = [ "param_" + sys.argv[2] ]
    else:
        keys = [ k for k in cdb.get_keys() if k.startswith("param_") ]

    for key in keys:
        print("%s = %s" % (key, cdb.get_string(key)))

cdb.clear()
