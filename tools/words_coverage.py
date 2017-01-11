#! /usr/bin/python3

import sys

coverage = float(sys.argv[1])

all = []
total = 0
for li in sys.stdin.readlines():
    count, word = li.strip().split(';')
    count = int(count)
    all.append((word, count))
    total += count

target = total * coverage
total = 0
for word, count in all:
    total += count
    print(word)
    if total > target: break
