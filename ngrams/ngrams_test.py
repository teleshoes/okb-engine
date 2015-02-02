#! /usr/bin/python3
# -*- coding: utf-8 -*-

import comp_ngrams
import random

random.seed(0)

gram_count = 50000
max_id = 3000
max_count = 100
verbose = False

grams = []
for i in range(0, gram_count):
    g = [ random.randint(1, max_id),
                    random.randint(1, max_id),
                    random.randint(1, max_id) ]
    count = random.randint(1, max_count)
    if verbose: print("N-gram:", g, count)
    grams.append((g, count))

e = comp_ngrams.NGramEncoder(word_nbits = 20, base_bits = 2, block_size = 128, verbose = verbose)

for g in grams:
    e.add_ngram(g[0], g[1])

b = e.get_bytes()

print("Bytes: %d" % len(b))

d = comp_ngrams.NGramDecoder(content = b, verbose = verbose)

for (gram, expected) in grams:
    count = d.search(gram)

    if count != expected:
        raise Exception("Count mismatch: %s %s != %d" % (gram, count, expected))
