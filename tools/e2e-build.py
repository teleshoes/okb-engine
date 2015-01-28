#! /usr/bin/python3
# -*- coding: utf-8 -*-

# Generate end-to-end test cases (curves + prediction engine)

# input = sorted n-gram file (descending order)

import sys, os
import getopt

import pickle
import gen_curve
import json

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import predict

os.chdir(os.path.dirname(__file__))

jsonfile = '../test/hello.json'
js = json.loads(open(jsonfile, "r").read())

error = 50
curviness = 150
lang = "en"
verbose = False
min_count = 10
max_err = None

opts, args =  getopt.getopt(sys.argv[1:], 'e:c:l:m:va:')
for o, a in opts:
    if o == "-e":  # error
        error = int(a)
    elif o == "-c":
        curviness = int(a)
    elif o == "-l":
        lang = a
    elif o == "-m":
        min_count = int(a)
    elif o == "-v":
        verbose = True
    elif o == "-a":
        max_err = float(a)
    else:
        print("Bad option: %s" % o)
        exit(1)

cache_dir = "/tmp/e2e-cache-%d-%d" % (error, curviness)
if not os.path.isdir(cache_dir): os.mkdir(cache_dir)


# read n-grams from stdin
wordcount = dict()
grams = []
curves = dict()
for line in sys.stdin.readlines():
    line = line.strip()
    if not line: continue

    (count, w1, w2, w3) = line.split(';')
    count = int(count)

    if len(w3) == 1: continue  # don't care about 1-letter words

    if w2 == '#NA':  # get word count from 1-grams
        wordcount[w3] = count
        continue

    if count < min_count: break

    if w1 == '#NA' and w2 != '#START': continue  # only 3-grams
    if w3 == '#TOTAL': continue  # no total

    # make sure we have curve sample for this word
    letters = gen_curve.word2keys(w3)

    if len(letters) < 3: continue  # two-letter words are boring too :-)

    if letters in curves: continue

    if w3 not in wordcount:
        sys.stderr.write("Warning: Can't get word count for %s\n" % w3)
        continue

    grams.append([count, w1, w2, w3])


    # try cache
    cache_file = os.path.join(cache_dir, letters)
    if os.path.exists(cache_file):
        curves[letters] = pickle.load(open(cache_file, 'rb'))
        continue

    # run curve plugin
    nb_samples = max(1, min(10, int(float(wordcount[w3]) / 50)))

    sys.stderr.write("Generating curves for %s (count=%d)\n" % (w3, nb_samples))

    curve_results = []
    for i in range(0, nb_samples):
        candidates = gen_curve.gen_curve(w3, js,
                                         error = error if i != 2 else 1,
                                         curviness = curviness if i != 1 else 1,
                                         lang = lang, retry = 30, verbose = verbose,
                                         max_err = max_err)
        curve_results.append(candidates)

    curves[letters] = curve_results

    pickle.dump(curve_results, open(cache_file + ".tmp", 'wb'))
    os.rename(cache_file + ".tmp", cache_file)


# run prediction engine
class Tools():
    def log(self, *args, **kwargs):
        sys.stderr.write("%s\n" % (' '.join(map(str, args))))

    def cf(self, key, default_value, cast = None):
        return default_value

p = predict.Predict(tools = Tools())

db_file = os.path.join(mydir, "../db/predict-%s.db" % lang)
p.set_dbfile(db_file)
p.load_db()

wc = 0
for (count, w1, w2, w3) in grams:
    last = [w1, w2]
    while last and last[0] in [ '#NA', '#START' ]: last.pop(0)
    tmp = ' '.join(last)

    p.update_surrounding(tmp, len(tmp))
    p.update_preedit("")
    p._update_last_words()

    letters = gen_curve.word2keys(w3)

    nb_samples = min(count, len(curves[letters]))
    if error == 0: nb_samples = 1

    for i in range(0, nb_samples):
        testid = "-".join([w1, w2, w3, str(i)])

        n = int(count / nb_samples)
        if i == nb_samples - 1: n = count - (nb_samples - 1) * n

        candidates = curves[letters][i]
        if len(candidates) <= 1: continue  # these will always be right

        scores = p._get_all_predict_scores([c["word"] for c in candidates])

        wc += 1
        for candidate in candidates:
            predict_score = scores.get(candidate["word"], None)

            if predict_score:
                predict_score = predict_score[1].get("scores", None)
            if predict_score:
                predict_score = ' '.join("%s=%s" % (n, s) for n, s in predict_score.items())

            print("%s;%d;%s;%s;%.2f;%s;%s;%s" % (testid, n,
                                                 1 if candidate["word"] == w3 else 0,
                                                 candidate["word"],
                                                 candidate["score"], candidate["star"], candidate["class"],
                                                 predict_score))
        print()

sys.stderr.write("N-grams: %d - test cases: %d\n" % (len(grams), wc))
