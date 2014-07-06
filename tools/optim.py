#!/usr/bin/python2
# -*- coding: utf-8 -*-

"""
Tries to find the best set of parameters to get the "best" results.
"best" is defined by the scoring function. The current one is pretty useless :-)
"""

import json
import subprocess
import sys, os
import unicodedata
import re
import math
import copy
import time
import fcntl
import getopt

CLI = "../cli/build/cli"
TRE_DIR = "../db"
TEST_DIR = "../test"
LOG = None  # "/tmp/optim.log"
OUT = "optim.log"

defparams = [
    [ "dist_max_start", 85, int, 0, 150 ],
    [ "dist_max_next", 110, int, 0, 150 ],
    [ "match_wait", 7, int, 4, 12 ],
    [ "max_angle", 45, int, 10, 90 ],
    [ "max_turn_error1", 20, int, 10, 60 ],
    [ "max_turn_error2", 150, int, 30, 145 ],
    [ "max_turn_error3", 80, int, 10, 145 ],
    [ "length_score_scale", 80, int, 30, 150 ],
    [ "curve_score_min_dist", 50, int, 20, 100 ],
    [ "score_pow", 2, float, 0.1, 10 ],
    [ "weight_distance", 1 ],  # reference
    [ "weight_cos", 4, float, 0.1, 10 ],
    [ "weight_length", 2, float, 0.1, 10 ],
    [ "weight_curve", 4, float, 0.1, 10 ],
    [ "weight_curve2", 0.2, float, 0.1, 10 ],
    [ "weight_turn", 2, float, 0.1, 10 ],
    [ "length_penalty", 0.001, float, -0.1, 0.1 ],
    [ "turn_threshold", 25, int, 10, 45 ],
    [ "turn_threshold2", 80, int, 45, 120 ],
    [ "max_turn_index_gap", 5, int, 2, 10],
    [ "curve_dist_threshold", 120, int, 30, 200 ],
    [ "small_segment_min_score", 0.35, float, 0.01, 0.9 ],
    [ "anisotropy_ratio", 1.25, float, 1, 2 ],
    [ "sharp_turn_penalty", 0.4, float, 0, 1 ],
    [ "curv_amin", 5, int, 1, 45 ],
    [ "curv_amax", 75, int, 45, 89 ],
    [ "curv_turnmax", 70, int, 40, 89 ],
    [ "max_active_scenarios", 50 ],  # no optimization, larger is always better
    [ "max_candidates", 30 ],  # idem
]

params = dict()
for p in defparams:
    if len(p) > 2:
        (name, default, type, min_value, max_value) = tuple(p)
        params[name] = dict(value = default, type = type, min = min_value, max = max_value)
    else:
        params[p[0]] = dict(value=p[1])



def dump(txt, id = ""):
    sep = '--8<----[' + id + ']' + '-' * (65 - len(id))
    print sep
    print txt
    print sep
    print

def log1(txt):
    if LOG is None: return
    with open(LOG, 'a') as f:
        f.write(txt)
        f.write("\n")

def run1(json_str, lang):
    log1(">>>> " + json_str)
    sp = subprocess.Popen([ CLI, os.path.join(TRE_DIR, "%s.tre" % lang) ], stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    (json_out, err) = sp.communicate(json_str)
    if sp.returncode != 0:
        dump(json_str)
        raise Exception("return code: %s" % sp.returncode)
    i = json_out.find('Result: {')
    if i > -1: json_out = json_out[i + 8:]
    log1("<<<< " + json_out)
    log1("")
    return json_out, err

def update_json(json_str, params):
    js = json.loads(json_str)
    js["params"] = dict([ (x, y["value"]) for x, y in params.items() ])
    return json.dumps(js)

def score1(json_str, expected, typ):
    # mot attendu premier = score > 0 (distance au prochain dans la liste)
    # sinon < 0 : somme des distance de ceux qui sont devant ! (x 10000)
    js = json.loads(json_str)

    score_ref = None
    others = []
    for c in js["candidates"]:
        score = c["score"]
        name = c["name"]
        if name == expected:
            score_ref = score
        else: others.append(score)

    # print "Detail:" + ",".join("%s: %.3f" % (c, c["score"]) for x in js["candidates"])

    if score_ref is None: return -9999999  # targeted word is not even found
    if not len(others): return -9999999  # too good to be true :-)

    average = sum(others) / len(others)
    stddev = math.sqrt(sum([(x - average) ** 2 for x in others]) / len(others))
    max_score = max(others)

    if typ == "stddev":
        score = score_ref - (average + stddev)
    elif typ.startswith("max"):
        if score_ref >= max_score:
            score = score_ref - max_score
        else:
            coef = 10 if typ == "max2" else 1
            score = - coef * sum([ c["score"] - score_ref for c in js["candidates"] if c["score"] > score_ref ])

    else: raise Exception("unknown score type: $typ")

    return score

def run_all(tests, params, typ, fail_on_bad_score = False, return_dict = None, silent = False):
    total, n = 0, 0
    for (word, json_in, lang) in tests:
        log1("# %s (%s)" % (word, lang))
        if not silent: print "%s (%s): " % (word, lang),
        sys.stdout.flush()
        json_in2 = update_json(json_in, params)
        json_out, err = run1(json_in2, lang)
        score = score1(json_out, word, typ)
        if not silent: print "%.3f - " % score,
        if score < -999999 and fail_on_bad_score:
            dump(json_out, "out")
            dump(err, "err")
            dump(json_in2, "in")
            raise Exception("negative score in reference test data: %s (type=%s, score=%d)" % (word, typ, score))
        total += score
        n += 1
        if return_dict is not None: return_dict[word] = score
    if not silent: print "OK"
    return total / n

def optim(pname, params, tests, typ):
    value = value0 = params[pname]["value"]
    min, max, type = params[pname]["min"], params[pname]["max"], params[pname]["type"]
    score0 = run_all(tests, params, typ)
    print "Optim: %s (value=%s, score=%s)" % (pname, value0, score0)

    scores = [ (value, score0) ]
    if type == int: step0 = 1
    else: step0 = 0.005 * (max - min)

    for dir in [ -1, 1 ]:
        last_score = score0
        bad_count = 0
        step = step0
        while True:
            new_value = value + dir * step
            if new_value < min or new_value > max: break

            params[pname]["value"] = new_value
            score = run_all(tests, params, typ)
            if score > last_score: bad_count = 0
            else: bad_count += 1

            if bad_count > 4: break

            scores.append( (new_value, score) )
            print "Try[%s] %s->%s score=%f" % (pname, value, new_value, score)

            last_score = score
            if step < (max - min) / 10: step *= 2
            elif type == int: step += int(1 + (max - min) / 10)
            else: step *= 1.2

    value, score = sorted(scores, key = lambda x: x[1])[-1]
    if score > score0:
        params[pname]["value"] = value
        print "===> Update[%s] %s->%s (score: %.3f -> %.3f)" % (pname, value0, value, score0, score)
        return score
    else:
        print "===> No change [%s]" % pname
        params[pname]["value"] = value0
        return score0

def params2str(p): return ' '.join([ "%s=%s" % (x, y["value"]) for x, y in sorted(p.items()) ])

def load_tests():
    tests = []
    l = os.listdir(TEST_DIR)
    for fname in [ x for x in l if x[-5:] == '.json']:
        lang = "en"
        letters = unicode(fname[:-5])

        mo = re.match('^([a-z][a-z])\-(.*)', letters)
        if mo: lang, letters = mo.group(1), mo.group(2)

        letters = ''.join(c for c in unicodedata.normalize('NFD', letters) if unicodedata.category(c) != 'Mn')
        letters = re.sub(r'[^a-z]', '', letters.lower())
        letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
        tests.append((letters, file(os.path.join(TEST_DIR, fname)).read(), lang))

    tests.sort(key = lambda x: x[0])
    return tests

if __name__ == "__main__":
    start = time.time()

    opts, args =  getopt.getopt(sys.argv[1:], 'l:')
    for o, a in opts:
        if o == "-l":
            LOG = a
        else:
            print "Bad option: %s", o
            exit(1)

    if len(args) < 1:
        print "usage:", os.path.basename(sys.argv[0]), " <score type>"

    typ = args[0]

    os.chdir(os.path.dirname(sys.argv[0]))
    print 'Parameters: ' + params2str(params)
    params0 = copy.deepcopy(params)

    # load test suite
    tests = load_tests()

    print "===== Reference run ====="
    detail0 = dict()
    max_score = score = score0 = run_all(tests, params, typ, fail_on_bad_score = True, return_dict = detail0)
    max_params = None
    print

    changed = True
    it = 0
    while changed:
        it += 1
        changed = False
        for p in params:
            if "type" in params[p]:
                print "===== Optimizing parameter '%s' =====" % p
                score = optim(p, params, tests, typ)
                if score > max_score:
                    max_score = score
                    max_params = copy.deepcopy(params)
                    changed = True

                print "Start  (%.3f): %s" % (score0, params2str(params0))
                print "Current(%.3f): %s" % (score, params2str(params))
                print "Best   (%.3f): %s" % (max_score, params2str(max_params))

    with open(OUT, "a") as f:
        detail_max = dict()
        run_all(tests, params, typ, return_dict = detail_max)
        fcntl.flock(f, fcntl.LOCK_EX)
        f.write('=' * 70 + '\n')
        f.write("Score type: %s - Time: [%s] - Duration: %d - Iterations: %d\n\n" % (typ, time.ctime(), int(time.time() - start), it))
        f.write("Start  (%.3f): %s\n" % (score0, params2str(params0)))
        f.write(" -> %s\n\n" % str(detail0))
        f.write("Best   (%.3f): %s\n" % (max_score, params2str(max_params)))
        f.write(" -> %s\n\n" % str(detail_max))
