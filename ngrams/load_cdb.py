#! /usr/bin/python3
# -*- coding: utf-8 -*-

import os, sys

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, 'build')
sys.path.insert(0, libdir)

import cdb

cdb.load(sys.argv[1])

l = cdb.get_keys()

print ("Count:", len(l))
