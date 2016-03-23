#! /usr/bin/python3
# -*- coding: utf-8 -*-

# usage: cat dictionary.txt | find_affixes [<min count>] > new_dictionary.txt

# This version only finds affixes with delimiters (quote or hyphen). Not suited for German.


import sys, re

words = set()

min_count = 25
if len(sys.argv) >= 2: min_count = int(sys.argv[1])

for word in sys.stdin.readlines():
    word = word.strip()
    words.add(word)

def find_affixes(words, affix_out = None, filter = None):
    affix = dict()
    words_out = set()

    for word in words:

        # prefixes
        mo1 = re.match(r'^([a-z]+[\'\-])(.*)$', word, re.I)
        if mo1:
            key = mo1.group(1)
            nword = mo1.group(2)
            if nword in words:
                if key not in affix: affix[key] = 0
                affix[key] += 1
            if filter and key in filter:
                word = nword

        # suffixes
        mo2 = re.match(r'^(.*)([\'\-][a-z]+)$', word, re.I)
        if mo2:
            key = mo2.group(2)
            nword = mo2.group(1)
            if nword in words:
                if key not in affix: affix[key] = 0
                affix[key] += 1
            if filter and key in filter:
                word = nword

        words_out.add(word)

    if affix_out is not None: affix_out.update(affix)

    return words_out

affix = dict()
find_affixes(words, affix_out = affix)

for k, v in list(affix.items()):
    if v < min_count: del affix[k]

words = find_affixes(words, filter = set(affix.keys()))

print('\n'.join(words))
if affix:
    print('\n'.join(affix.keys()))
