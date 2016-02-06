#! /usr/bin/python

import sys, glob, os, re, json

ftest_dir = sys.argv[1]

os.chdir(ftest_dir)

files = glob.glob('*.predict.log')

for file in files:
    id = re.sub(r'\..*$', '', file)
    print id
    (_, expected, actual, ok) = open(file, 'r').readline().strip().split(' ')

    js = json.load(open("%s.json" % id, 'r'))
