#! /usr/bin/python3

import sys
total = 0
all = list()
for li in sys.stdin.readlines():
    count, word = li.strip().split(';')
    count = int(count)
    total += count
    all.append((count, word))

n = 0
t = 0
for count, word in all:
    n += 1
    t += count
    print("%6d : %20s : %9d - %9d : %6.2f%%" % (n, word, count, t, 100. * t / total))
