#! /usr/bin/python3

import sys, os

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import backend

db = backend.FslmCdbBackend(sys.argv[1])
db.file_export(sys.stdout)
