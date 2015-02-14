#! /usr/bin/python3

# split corpus text based on automatic language detection (very basic stuff)

import sys
import os
import re

langs = dict()
for langf in sys.argv[1:]:
    with open(langf, encoding = "utf-8") as f:
        langs[os.path.basename(langf)] = set(f.read().lower().split('\n'))

for line in sys.stdin.readlines():
    line = line.strip()
    l_line = line.lower()
    total = dict()
    for l in langs.keys(): total[l] = 0

    line = re.sub('[^\w\-\']+', ' ', line)
    for word in l_line.split(' '):
        if word:
            for l in langs.keys():
                if word in langs[l]: total[l] += 1

    max_count = max(total.values())
    max_lang = [ l for l in langs.keys() if total[l] == max_count ]
    if len(max_lang) == 1:
        print("[%s] %s" % (max_lang[0], line))
