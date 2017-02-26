#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os
import pickle
import unicodedata
import re
import shutil

src_dir = sys.argv[1]
dest_dir = sys.argv[2]

if len(sys.argv) >= 4: min_ts = int(sys.argv[3])
else: min_ts = 0

records = pickle.load(open(os.path.join(src_dir, "ftest-all.pck"), 'rb'))

def word2letters(word):
    letters = ''.join(c for c in unicodedata.normalize('NFD', word.lower()) if unicodedata.category(c) != 'Mn')
    letters = re.sub(r'[^a-z]', '', letters.lower())
    letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
    return letters

for t in records:
    id = t["id"]
    exp = t["expected"]
    lang = t["lang"]

    if min_ts:
        ts = int(id.split("-")[0])
        if ts < min_ts: continue

    src = os.path.join(src_dir, "%s.in.json" % id)
    dest = os.path.join(dest_dir, "%s-%s-%s.json" % (lang, word2letters(exp), id))

    print("%s -> %s" % (src, dest))
    shutil.copyfile(src, dest)
