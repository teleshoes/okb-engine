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
import unicodedata

affixes_file = None
debug = clean = False
opts, args =  getopt.getopt(sys.argv[1:], 'cda:')
for o, a in opts:
    if o == "-d":
        debug = True
    elif o == "-c":
        clean = True
    elif o == "-a":
        affixes_file = a
    else:
        print("Bad option: %s", o)
        exit(1)

if len(args) < 1:
    print("Usage: ", os.path.basename(__file__), " [<opts>] <dictionary file>")
    print(" corpus data is read from stdin (normal text file)")
    print(" words not included in dictionary file are ignored (text file, one word per line)")
    print(" result is written as semicolon separated CSV to stdout")
    print(" options:")
    print(" -d : only outputs cleaned sentences (for debug)")
    print(" -c : outputs cleaned sentences (one per lines)")
    print(" -a <file> : load affixes from file (only prefix/suffix separated by punctuation sign atm)")
    exit(255)

dictfile = args[0]

class CorpusImporter:
    def __init__(self, debug = False, clean = False):
        self.grams = dict()
        self.sentence = []
        self.flags = dict()
        self.word_used = set()
        self.size = 0
        self.debug = debug
        self.clean = clean
        self.dedupe = set()
        self.affixes = set()

    def load_words(self, words):
        self.words = set()
        self.islower = dict()
        self.noaccents = dict()
        for word in words:
            word = word.strip()  # .decode('utf-8')
            if not word: continue  # skip blank lines
            if not re.match(r'^[a-zA-Z\.\'\-\u0080-\u023F]+$', word):
                raise Exception("Input dictionary contains words with invalid characters: %s" % word)
            self.words.add(word)
            if word.lower() != word:
                self.islower[word.lower()] = word

            noaccents = ''.join(c for c in unicodedata.normalize('NFD', word) if unicodedata.category(c) != 'Mn')
            if noaccents != word:
                self.noaccents[noaccents] = word

    def load_affixes(self, fname):
        self.affixes = [ w for w in re.split('\s+', open(affixes_file, 'r').read()) if w ]

    def parse_line(self, line):
        if self.debug: print(line)
        self.size += len(line)
        line = line.strip()

        if not re.match(r'^[\ \!-\~\t\r\n\'\u0080-\u023F\u20AC]*$', line):
            raise Exception("Line contains invalid characters: %s" % line)

        if re.match(r'^\s*$', line):
            self.next_sentence()
            return

        while line:
            line = line.strip()

            # consume non word characters
            mo = re.match(r'^[^\w\'\-]+', line)
            if mo:
                if re.match(r'[\.:;\!\?]', mo.group(0)): self.next_sentence()
                line = line[mo.end(0):]

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
                self.add_word('#ERR')
                continue

            # read next word
            line = line.strip()
            mo = re.match(r'^[\w\'\-]+', line)
            word = mo.group(0)
            line = line[mo.end(0):]

            tokens = self.get_words(word, first = not self.sentence)
            if tokens:
                if "c'est" in tokens: raise Exception("plop")
                for token in tokens: self.add_word(token)
            else:
                # unknown word
                self.add_word('#ERR')

    def get_words(self, word, first = False):
        word = re.sub(r"^[-']+", '', word)
        word = re.sub(r"[-']+$", '', word)
        if not word: return []  # just noise

        # find compound word
        if word.find("'") > -1 or word.find("-") > -1:

            # prefixes
            mo = re.match(r'^([^\-\']+[\-\'])(.+)$', word)
            if mo:
                prefix, rest = mo.group(1), mo.group(2)
                if first and prefix.lower() in self.affixes and prefix[0].isupper() and (prefix[1:].islower() or len(prefix) <= 2):
                    prefix = prefix.lower()  # uncapitalize prefix at the beginning of a sentence
                elif prefix.lower() in self.affixes and prefix not in self.affixes:
                    prefix = prefix.lower()  # fall back mode for bad capitalization (@TODO insert a #ERR tag ?)

                if prefix in self.affixes:
                    tokens = self.get_words(rest, first = False)
                    if tokens: return [ prefix ] + tokens
                    self.words.add(rest)
                    return [ prefix, rest ]

            # suffixes
            mo = re.match(r'^(.+)([\-\'][^\-\']+)$', word)
            if mo:
                rest, suffix = mo.group(1), mo.group(2)
                if suffix in self.affixes:
                    tokens = self.get_words(rest, first = first)
                    if tokens: return tokens + [ suffix ]
                    if first and rest[0].isupper() and rest[1:].islower(): rest = rest.lower()
                    self.words.add(rest)
                    return [ rest, suffix ]

        # capitalized word at the beginning of a sentence
        if word.lower() in self.words and first and word[0].isupper() and word[1:].islower():
            return [ word.lower() ]

        # unchanged word
        if word in self.words:
            return [ word ]

        # other capitalized
        if word.lower() in self.words:
            return [ word.lower() ]

        # lowercase version of a proper noun
        if word in self.islower:
            return [ self.islower[word] ]

        # word with forgotten diacritics (occurs sometime in my French corpus file)
        if word in self.noaccents:
            return [ self.noaccents[word] ]

        # no match
        return None

    def add_word(self, word, flags = dict()):
        for flag in flags:
            if flag not in self.flags: self.flags[flag] = 0
            self.flags[flag] += 1

        if isinstance(word, (list, tuple)):
            self.sentence.extend(list(word))
        else:
            self.sentence.append(word)

    def next_sentence(self):
        if not self.sentence: return
        sentence = self.sentence
        if self.clean: print(' '.join(sentence))

        # this is the right place for deduplication because upstream processes
        # have no notion of matched word
        dedupe = False
        error_count = sentence.count('#ERR')
        if error_count > len(sentence) / 2: dedupe = True
        # @todo handle other cases
        if dedupe:
            key = ' '.join(sentence)
            if key in self.dedupe: return
            self.dedupe.add(key)


        if self.debug: print("WORDS:", sentence)
        roll = [ '#START' ] * 3

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
        self.flags = dict()

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

# 1.5) load affixesif affixes_file:
if affixes_file: ci.load_affixes(affixes_file)

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
