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
import getopt

debug = clean = False
opts, args =  getopt.getopt(sys.argv[1:], 'cd')
for o, a in opts:
    if o == "-d":
        debug = True
    elif o == "-c":
        clean = True
    else:
        print("Bad option: %s", o)
        exit(1)

if len(args) < 1:
    print("Usage: ", os.path.basename(__file__), " [<opts>] <dictionary file>")
    print(" corpus data is read from stdin (normal text file)")
    print(" words not included in dictionary file are ignored (text file, one word per line)")
    print(" result is written as semicolon separated CSV to stdout")
    print(" option -d only outputs cleaned sentences (for debug)")
    print(" option -c outputs cleaned sentences (one per lines)")
    exit(255)

dictfile = args[0]

class CorpusImporter:
    def __init__(self, debug = False, clean = False):
        self.grams = dict()
        self.sentence = []
        self.word_used = set()
        self.size = 0
        self.debug = debug
        self.clean = clean
        self.dejavu = set()

    def load_words(self, words):
        id = 2
        self.words = set()
        for word in words:
            id += 1
            word = word.strip()  # .decode('utf-8')
            self.words.add(word)

    def parse_line(self, line):
        if self.debug: print(line)
        self.size += len(line)
        line = line.strip()
        if re.match(r'^\s+$', line):
            self.next_sentence()
            return

        first_word = True
        while line:
            line = line.strip()

            # consume non word characters
            mo = re.match(r'^[^\w\'\-]+', line)
            if mo:
                if re.match(r'[\.:;\!\?]', mo.group(0)): self.next_sentence()
                line = line[mo.end(0):]
                first_word = True

            if not line: break

            # find acronyms with dots and skip them
            # (in this case a dot does not mean a new sentence)
            # ok this thing may eat one letter words at the end of sentences
            # but word prediction for one letter words is not important
            line = line.strip()
            mo = re.match(r'^([A-Z]\.\-{,1})+', line)  # i've got things like A.-B. in my text samples
            if mo:
                line = line[mo.end(0):]
                if re.match(r'^[A-Z]\b', line): line = line[1:]  # last letter without "."
                self.next_sentence()
                self.sentence.append('#ERR')
                continue

            # read next word
            line = line.strip()
            mo = re.match(r'^[\w\'\-]+', line)
            word = mo.group(0)
            line = line[mo.end(0):]

            if word in self.words:
                self.sentence.append(word)
                first_word = False
                continue

            if word.lower() in self.words:
                if first_word: self.next_sentence()
                self.sentence.append(word.lower())
                first_word = False
                continue

            first_word = False

            # english "'s"
            mo1 = re.match(r'^([a-zA-Z]+)\'[a-z]$', word)
            if mo1 and mo1.group(1) in self.words:
                self.sentence.append(mo1.group(1))
                continue

            # in word hyphen
            # @todo: we should find a way to "auto-hyphen" when typing (n-grams tagging?)
            l = word.split('-')
            if not [ w for w in l if w not in self.words ]:
                self.sentence.extend(l)
                continue
            if not [ w for w in l if w.lower() not in self.words ]:
                self.sentence.extend([w.lower() for w in l])
                continue

            # unknown word
            self.next_sentence()
            self.sentence.append('#ERR')

    def next_sentence(self):
        if not self.sentence: return
        sentence = self.sentence
        txt = ' '.join(sentence)
        if self.clean and sentence != [ '#ERR' ]: print(txt)
        self.sentence = [ ]

        if txt in self.dejavu: return
        self.dejavu.add(txt)

        if self.debug: print("WORDS:", sentence)

        roll = [ '#START' ] * 3
        if sentence[0] == '#ERR':
            sentence.pop(0)
            roll = [ '#ERR' ] * 3

        # if len(sentence) < 2: return  # one-word sentences are suspicious :-) -> not at all!

        for word in sentence:
            self.word_used.add(word)
            roll.pop(0)
            roll.append(word)

            # 3-grams
            if roll[1] == '#START':
                # a 3-gram is meaningless for a single word at the beginning of a sentence
                # and it's no use having both [ #START, #START, word ] and [ #NA, #START, word ]
                pass
            else:
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
ci = CorpusImporter(debug = debug, clean = clean)
if dictfile[-3:] == '.gz': f = gzip.open(dictfile)
else: f = open(dictfile)
ci.load_words(f.readlines())

# 2) import corpus (in-memory, not meant to be efficient)
line = sys.stdin.readline()
last = start = time.time()
while line:
    ci.parse_line(line)
    line = sys.stdin.readline()

    now = time.time()
    if now >= last + 5 and not debug:
        sys.stderr.write("[%d] %s\n" % (int(now - start), ci.get_stats()))
        last = now

# 3) result to stdout CSV
if not debug and not clean: ci.print_grams()
