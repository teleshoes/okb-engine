#! /usr/bin/python3
# -*- coding: utf-8 -*-

IT = 100
N = 1000000
N2 = 10000
N3 = 1000

import random
import tempfile
import sys, os

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, 'build')
sys.path.insert(0, libdir)

import cdb

random.seed(0)

(h, tmp) = tempfile.mkstemp()

exp = dict()

for it in range(0, IT):
    print(it)
    cdb.load(tmp)
    for i in range(0, N2):
        index = random.randint(1, N)
        if index in exp:
            actual = cdb.get_string(exp[index][0])
            if actual != exp[index][1]: raise Exception("Bad value !")

        key = "key-%d" % index
        value = "value-%d" % index
        exp[index] = key, value
        cdb.set_string(key, value)

    for i in range(0, N3):
        index = random.randint(1, N)
        if index not in exp: continue
        cdb.rm(exp[index][0])
        del exp[index]

    cdb.save()
    cdb.clear()
