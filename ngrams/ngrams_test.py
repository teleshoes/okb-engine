#! /usr/bin/python3
# -*- coding: utf-8 -*-

import fslm_ngrams
import random
import math

random.seed(0)

gram_count = 50
max_id = 3000
max_count = 100
verbose = True

grams = []
dejavu = set()
for i in range(0, gram_count):
    n = int(math.log10(random.randint(1, 999))) + 1
    g = tuple([ random.randint(1, max_id) for x in range(0, n) ])
    if g in dejavu: continue
    dejavu.add(g)
    count = random.randint(1, max_count)
    if verbose: print("N-gram:", g, count)
    grams.append((g, count))

e = fslm_ngrams.NGramEncoder(word_nbits = 20, base_bits = 2, block_size = 128, verbose = verbose)

for g in grams:
    e.add_ngram(g[0], g[1])

b = e.get_bytes()

print("Bytes: %d" % len(b))

d = fslm_ngrams.NGramDecoder(content = b, verbose = verbose)

for (gram, expected) in grams:
    count = d.search(gram)

    if count != expected:
        raise Exception("Count mismatch: %s %s != %d" % (gram, count, expected))
