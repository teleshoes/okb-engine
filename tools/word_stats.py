#! /usr/bin/python3

# example:
#
# lbzip2 -dc < $WORK_DIR/grams-fr-learn.csv.bz2 | grep '#NA;#NA' | cut -d ";" -f 1,4 | sort -rn > /tmp/words-fr.txt
#
# lbzip2 -d < sampletext.txt.bz2 | clean_corpus.py | import_corpus.py $WORK_DIR/fr-full.dict | \
#   grep '#NA;#NA' | cut -d ";" -f 1,4 | word_stats.py /tmp/words-fr.txt

import sys

words = dict()
f = open(sys.argv[1], 'r')
for line in f:
    count, word = line.strip().split(';')
    if word.find('#') > -1: continue
    words[word] = dict(dict_count = int(count), text_count = 0)
f.close()

total_count = total_found_count = 0
for line in sys.stdin:
    count, word = line.strip().split(';')
    if word.find('#') > -1: continue

    count = int(count)
    total_count += count

    if word not in words: continue

    words[word]["text_count"] += count
    total_found_count += count

print("# words: %d, total_count: %d, found_count: %d (%.2f%%)" % (len(words), total_count,
                                                                  total_found_count,
                                                                  100. * total_found_count / total_count))

wordl = list(words.keys())
wordl.sort(key = lambda x: words[x]["dict_count"], reverse = True)

part_count = 0
idx = 0
for word in wordl:
    part_count += words[word]["text_count"]
    idx += 1
    print("%6d : %20s%1s : %9d - %9d : %6.2f%% / %6.2f%%" % (idx, word,
                                                             "*" if words[word]["text_count"] else " ",
                                                             part_count,
                                                             total_found_count - part_count,
                                                             100. * part_count / total_found_count,
                                                             100. * part_count / total_count))
