#! /usr/bin/python3
# -*- coding: utf-8 -*-

# End-to-end testing with actual text corpus and prediction engine code
# Use generated input strokes for realistic curve matching output

# This allows to simulate new prediction strategies (such as backtracking)

# input (stdin) = cleaned text corpus (on sentence per line, word separated by
#                 spaces and no punctuation)
# example: cat <corpus file> | import_corpus.py -c <word file> | e2e-test-predict.py -d <db file>


import getopt, os, sys, json, time
import pickle

import gen_curve

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import predict

error = 40
curviness = 150
lang = "en"
max_err = .2
db_file = None
quiet = False
max_count = None
max_rank = 8

opts, args =  getopt.getopt(sys.argv[1:], 'sl:d:m:qe:c:')
for o, a in opts:
    if o == "-l":
        lang = a
    elif o == "-s":
        learn = True
    elif o == "-d":
        db_file = a
    elif o == "-q":
        quiet = True
    elif o == "-m":
        max_count = int(a)
    elif o == "-e":
        error = int(a)
    elif o == "-c":
        curviness = int(a)
    else:
        print("Bad option: %s" % o)
        exit(1)


class MockTools:
    def __init__(self, quiet = False): self.quiet = quiet

    def cf(self, key, default_value, cast = None):
        value = default_value
        if cast: value = cast(value)
        return value

    def log(self, *args, **kwargs):
        if self.quiet: return
        message = ' '.join(map(str, args))
        print(message)


p = predict.Predict(tools = MockTools(quiet))
if not db_file: db_file = os.path.join(mydir, "../db/predict-%s.db" % lang)
p.set_dbfile(db_file)
p.load_db()

os.chdir(os.path.dirname(__file__))

jsonfile = '../test/hello.json'
js = json.loads(open(jsonfile, "r").read())

cache_dir = "/tmp/e2e-cache-%d-%d" % (error, curviness)
if not os.path.isdir(cache_dir): os.mkdir(cache_dir)

def get_curve_result(word, index = 0):
    global js, p

    letters = gen_curve.word2keys(word)
    if len(letters) == 1:
        d = dict(word = word, name = letters, score = 1.0, star = False)
        d["class"] = 0
        return [ d ]

    # try cache
    cache_file = os.path.join(cache_dir, letters)
    if os.path.exists(cache_file):
        curve_results = pickle.load(open(cache_file, 'rb'))
    else:

        # run curve plugin
        nb_samples = 10

        sys.stderr.write("Generating curves for %s (count=%d)\n" % (word, nb_samples))

        curve_results = []
        for i in range(0, nb_samples):
            candidates = gen_curve.gen_curve(word, js,
                                             error = error if i != 2 else 1,
                                             curviness = curviness if i != 1 else 1,
                                             lang = lang, retry = 30, verbose = False,
                                             max_err = max_err, ignore_errors = True, max_rank = max_rank)
            if candidates:
                curve_results.append(candidates)

        pickle.dump(curve_results, open(cache_file + ".tmp", 'wb'))
        os.rename(cache_file + ".tmp", cache_file)

    if not curve_results: return None
    result = curve_results[index % len(curve_results)]
    return result

index = 1
lcount = count = ok = 0
std_ok = bt_count = bad_bt_count = 0
ts = time.time()

def stats():
    print("***** Count=%d OK=%d bt=%d bad_bt=%d (%.1f%%) - OK_nobt=%d (%.1f%%)" %
          (count, ok, bt_count, bad_bt_count, 100.0 * ok / count, std_ok, 100.0 * std_ok / count))

for line in sys.stdin:
    words = line.strip().split(' ')

    lcount += 1
    p.log("==> [#%d] Words: %s" % (lcount, ' '.join(words)))

    context = [ ]
    actual  = [ ]
    for word in words:
        index += 1
        if word == '#ERR':
            context = [ '#ERR' ]
            actual  = [ '#ERR' ]
            continue

        # if context and context[0] == '#ERR' and len(context) < 3: continue

        word_db = p.db.get_words([ word ], [ word ] if not context else None)
        if word not in word_db:
            # unknown word
            p.log("Unknown word: %s" % word)
            context = [ ]
            actual = [ ]
            continue

        candidates = get_curve_result(word, index)

        if not candidates:
            context = [ '#ERR' ]
            actual  = [ '#ERR' ]
            continue

        tmp = ' '.join(context)
        p.update_surrounding(tmp, len(tmp))
        p.update_preedit("")
        p._update_last_words()

        candidates = [ (d["word"], d["score"], d["class"], d["star"], d["word"]) for d in candidates ]  # emulate old format

        guessed_word = p.guess(candidates)
        if word == guessed_word: std_ok += 1

        bt = p.backtrack(verbose = not quiet, expected = (actual[-1] if actual else None, word))
        if bt:
            # word N-1 has been replaced
            msg = "Backtrack: [%s] %s %s ==> [%s] %s %s (expected: %s)" % (
                ' '.join(context[-4:-1]) or "-", context[-1], guessed_word,
                ' '.join(context[-4:-1]) or "-", bt[0], bt[1],
                ' '.join(actual[-4:] + [ word ]))

            previous_guessed_word = bt[0]
            bt_count += 1
            if previous_guessed_word == actual[-1]:
                # we actually corrected a mistake [bad->good]
                ok += 1
                print("%s (OK)" % msg)

            elif context[-1] == actual[-1]:
                # we changed N-1 even if it was all right --> worst case [good->bad]
                ok -= 1
                bad_bt_count += 1
                print("%s (FAIL+)" % msg)

            else:
                # we just chose another bad word :-)
                print("%s (FAIL)" % msg)
                bad_bt_count += 1
                pass

            guessed_word = bt[1]
            context[-1] = previous_guessed_word
            p.log()

        count += 1
        if word == guessed_word: ok += 1

        context.append(guessed_word)
        actual.append(word)

    p.log("Sentence : context", context)
    p.log("Sentence :  actual", actual)

    if time.time() > ts + 10:
        ts += 10
        stats()

    if max_count and count >= max_count: break

stats()
