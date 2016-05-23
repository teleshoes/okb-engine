#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" factory reset  """

import sys, os

mydir = os.path.dirname(os.path.abspath(__file__))
mydir = libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

from backend import FslmCdbBackend

if len(sys.argv) < 2:
    print("usage: " + os.path.basename(__file__) + " <db file>")
    exit(1)

db = FslmCdbBackend(sys.argv[1])
db.factory_reset()
