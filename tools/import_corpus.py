#! /usr/bin/python3
# -*- coding: utf-8 -*-

"""
Loads a text corpus and generate n-grams database (for n in [1..3])
The result can be imported into a db with load_dbm.py or load_sqlite.py
"""

import sys, os
import time
import gzip
import re

if len(sys.argv) < 2:
    print("Usage: ", os.path.basename(__file__), " <dictionary file>")
    print(" corpus data is read from stdin (normal text file)")
    print(" words not included in dictionary file are ignored (text file, one word per line)")
    print(" result is written as semicolon separated CSV to stdout")
    exit(255)

dictfile = sys.argv[1]

class CorpusImporter:
    def __init__(self):
        self.grams = dict()
        self.sentence = []
        self.word_used = set()
        self.size = 0

    def load_words(self, words):
        id = 2
        self.words = set()
        for word in words:
            id += 1
            word = word.strip().decode('utf-8')
            self.words.add(word)

    def parse_line(self, line):
        self.size += len(line)
        line = line.strip()
        if re.match(r'^\s+$', line):
            self.next_sentence()
            return

        first_word = True
        while line:
            # consume non word characters
            mo = re.match(r'^[^\w\'\-]+', line)
            if mo:
                if re.match(r'[\.:;\!\?]', mo.group(0)): self.next_sentence()
                line = line[mo.end(0):]
                first_word = True

            if not line: break

            # read next word
            mo = re.match(r'[\w\'\-]+', line)
            word = mo.group(0)
            line = line[mo.end(0):]

            if word in self.words:
                self.sentence.append(word)
                continue

            if word.lower() in self.words:
                if first_word: self.next_sentence()
                self.sentence.append(word.lower())
                continue

            # english "'s"
            mo1 = re.match(r'^([a-zA-Z]+)\'[a-z]$', word)
            if mo1 and mo1.group(1) in self.words:
                self.sentence.append(mo1.group(1))
                continue

            # in word hyphen
            l = word.split('-')
            if not [ w for w in l if w not in self.words ]:
                self.sentence.extend(l)
                continue

            # unknown word
            self.next_sentence()
            self.sentence.append('#ERR')

    def next_sentence(self):
        if not self.sentence: return
        sentence = self.sentence
        self.sentence = [ ]

        roll = [ '#START' ] * 3
        if sentence[0] == '#ERR':
            sentence.pop(0)
            roll = [ '#ERR' ] * 3

        if len(sentence) < 2: return  # one-word sentences are suspicious :-)

        # print(" >>>", sentence)  # QQQ
        for word in sentence:
            self.word_used.add(word)
            roll.pop(0)
            roll.append(word)

            # 3-grams
            self.count(roll)

            # 2-grams
            self.count(roll[1:3])

            # 1-grams (also called "words")
            self.count([roll[2]])

        self.sentence = [ ]

    def count(self, elts):
        elts = elts[:]
        if '#ERR' in elts: return
        self.count2(elts)
        elts.pop()
        self.count2(elts + [ '#TOTAL' ])

    def count2(self, elts):
        elts = elts[:]
        while len(elts) < 3: elts.insert(0, '#NA')

        key = ';'.join(elts)
        if key not in self.grams: self.grams[key] = 0
        self.grams[key] += 1

    def get_stats(self):
        return ("%d/%d words, %d n-grams, read %d MB" % (len(self.word_used), len(self.words), len(self.grams), int(self.size / 1024 / 1024)))

    def print_grams(self):
        for key, count in self.grams.items():
            print("%d;%s" % (count, key))

# 1) load dictionary
ci = CorpusImporter()
if dictfile[-3:] == '.gz': f = gzip.open(dictfile)
else: f = file(dictfile)
ci.load_words(f.readlines())

# 2) import corpus (in-memory, not meant to be efficient)
line = sys.stdin.readline()
last = start = time.time()
while line:
    ci.parse_line(line)
    line = sys.stdin.readline()

    now = time.time()
    if now >= last + 5:
        sys.stderr.write("[%d] %s\n" % (int(now - start), ci.get_stats()))
        last = now

# 3) result to stdout CSV
ci.print_grams()
