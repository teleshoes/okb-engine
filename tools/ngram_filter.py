#! /usr/bin/python3

import sys, os
import getopt

opts, args =  getopt.getopt(sys.argv[1:], 'l:')
logfile = None
for o, a in opts:
    if o == "-l": logfile = open(a, 'w')
    else: raise Exception("bad opt:", o)


if len(args) < 2:
    print("usage:", os.path.basename(__file__), "<word coverage [0-1]> <ngram coverage [0-1]>")
    print(" input/ouput ngram files on stdin/stdout (sorted by count)")
    exit(1)

word_coverage, ngram_coverage = float(args[0]), float(args[1])
if word_coverage <= 0 or word_coverage > 1: raise Exception("Word coverage must be between 0 and 1")
if ngram_coverage <= 0 or ngram_coverage > 1: raise Exception("N-gram coverage must be between 0 and 1")

word_sum = ngram_sum = 0
words = dict()
ngrams = [ ]

last_count = 0
for line in sys.stdin:
    count, w1, w2, w3 = line.strip().split(';')
    count = int(count)

    if count < last_count: raise Exception("Input is not sorted by count")
    last_count = count

    if w1 == '#NA' and w2 == '#NA' and w3 != '#TOTAL':
        words[w3] = count
        word_sum += count

    ngrams.append((count, ';'.join([w1, w2, w3])))
    ngram_sum += count

def say(x):
    global logfile
    if logfile: logfile.write(x + "\n")
    sys.stderr.write(x + "\n")

# remove rare words
new_word_sum = word_sum
words0 = dict(words)
word_list = sorted(words.keys(), key = lambda w: words[w])
while new_word_sum > word_sum * word_coverage:
    word = word_list.pop(0)
    new_word_sum -= words[word]
    del words[word]

say("Words: %d -> %d (%.2f%%), coverage=%.2f%%" %
    (len(words0), len(words), 100.0 * len(words) / len(words0), 100.0 * new_word_sum / word_sum))

# remove n-grams using removed words
new_ngram_sum = ngram_sum
ngram_count0 = len(ngrams)
new_ngrams = [ ]
for count, ngram in ngrams:
    if [ w for w in ngram.split(';') if w not in words and not w.startswith('#') ]:
        new_ngram_sum -= count
    else:
        new_ngrams.append((count, ngram))

ngrams = new_ngrams
new_ngrams = None

say("N-grams: %d -> %d (%.2f%%), coverage=%.2f%%" %
    (ngram_count0, len(ngrams), 100.0 * len(ngrams) / ngram_count0, 100.0 * new_ngram_sum / ngram_sum))

# remove rare n-grams
for i in range(0, len(ngrams)):
    if new_ngram_sum <= ngram_sum * ngram_coverage: break
    count, ngram = ngrams[i]
    new_ngram_sum -= count
ngrams = ngrams[i:]

say("N-grams: %d -> %d (%.2f%%), coverage=%.2f%%" %
    (ngram_count0, len(ngrams), 100.0 * len(ngrams) / ngram_count0, 100.0 * new_ngram_sum / ngram_sum))

for count, ngram in ngrams:
    print("%s;%s" % (count, ngram))

if logfile: logfile.close()
