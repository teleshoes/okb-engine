#! /usr/bin/python3

import sys
import re
current = ""

dedupe = dict()

for line in sys.stdin:
    line = re.sub(r'\s\s+', ' ', line.strip())

    if line not in dedupe: dedupe[line] = 0
    dedupe[line] += 1
    if dedupe[line] > 20: continue  # ignore signatures / headers / etc.

    if not re.search(r'\w', line):
        if current: print(current + " .")
        current = ""
        continue

    if not current:
        current = line
        continue

    join_ok = True
    if not re.search(r'\w[\s\,]*$', current): join_ok = False
    if not re.search(r'^\w', line): join_ok = False
    if line[0].lower() != line[0]: join_ok = False

    if join_ok:
        current += " " + line
    else:
        print(current + " .")
        current = line

print(current + " .")
