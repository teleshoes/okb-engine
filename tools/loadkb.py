#! /usr/bin/python2
# -*- coding: utf-8 -*-

"""
This script creates .tre files while contains all possible words as a tree (the n-th level contains the n-th letter in the word)
It's maybe a trie (https://en.wikipedia.org/wiki/Trie) but i'm not sure :-)

File extension is called ".tre" because duplicate letters are only included once.
"""

import sys
import gribouille

t = gribouille.LetterTree()

t.beginLoad()

n = 0
for word in sys.stdin.readlines():
    word = word.strip()
    if not word: continue

    t.addWord(word)

    n += 1
    if not n % 1000: print n

t.endLoad()

t.saveFile(sys.argv[1])
