#! /usr/bin/python3
# -*- coding: utf-8 -*-

# usage: cat dictionary.txt | find_affixes [<min count>] > new_dictionary.txt

# This version only finds simple prefixes & suffixes with delimiters (quote or hyphen). Not suited for German.


import sys, re

words = set()

min_count = 25
if len(sys.argv) >= 2: min_count = int(sys.argv[1])

for word in sys.stdin.readlines():
    word = word.strip()
    words.add(word)

def find_affixes(words):
    affix = dict()

    for word in words:

        # prefixes
        mo1 = re.match(r'^([a-z]+[\'\-])(.*)$', word, re.I)
        if mo1:
            key = mo1.group(1)
            nword = mo1.group(2)
            if nword in words:
                if key not in affix: affix[key] = 0
                affix[key] += 1

        # suffixes
        mo2 = re.match(r'^(.*)([\'\-][a-z]+)$', word, re.I)
        if mo2:
            key = mo2.group(2)
            nword = mo2.group(1)
            if nword in words:
                if key not in affix: affix[key] = 0
                affix[key] += 1

    return affix

affix = find_affixes(words)

for k, v in list(affix.items()):
    if v < min_count: del affix[k]

print('\n'.join(affix.keys()))
