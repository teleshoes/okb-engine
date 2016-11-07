#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os
import re

mydir = os.path.dirname(os.path.abspath(__file__))
mydir = libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import language_model, backend


if len(sys.argv) > 1: f = open(sys.argv[1], 'r')
else: f = sys.stdin

summary = expected = None

for line in f.readlines():
    line = line.strip()

    if line.startswith('Summary:'):
        summary = line
        expected = re.split(r'\s+', line)[2]

    if line.startswith('ID: '):
        id = re.search(r'^ID:\s+(\S+)', line).group(1)
        context = re.search(r'Context:\s+\[([^\[\]]*)\]', line).group(1)
        context = re.sub('\s', '', context)
        context = [ re.sub(r'^\'(.*)\'', r'\1', x) for x in context.split(',') ]
        context = list(reversed(context))

        speed = int(re.search(r'Speed:\s*(\d+)', line).group(1))
        db_file = re.search(r'DB:\s*(\S+)', line).group(1)

    if line.startswith('Candidates:'):
        line = re.sub(r',', '', line)
        clist = line.split(' ')
        clist.pop(0)
        candidates = dict()
        for c in clist:
            mo = re.match(r'^(.*)\[([0-9\.]+)\]$', c)
            candidates[mo.group(1)] = float(mo.group(2))

if summary: print(summary)
db = backend.FslmCdbBackend(db_file)
lm = language_model.LanguageModel(db, tools = None, debug = True, dummy = False)
guesses = lm.guess(context, candidates, id, speed = speed,
                   verbose = True, expected_test = expected, details_out = None)
