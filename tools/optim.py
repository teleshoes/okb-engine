#!/usr/bin/python3
# -*- coding: utf-8 -*-

"""
This script tries to find the best set of parameters to get the "best" results.
"best" is defined by the scoring function. The current one is pretty lame :-)
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
from multiprocessing.pool import ThreadPool
import multiprocessing
import configparser as ConfigParser
import params

CLI = "../cli/build/cli"
TRE_DIR = "../db"
TEST_DIR = "../test"
LOG = None  # "/tmp/optim.log"
OUT = "optim.log"

def get_params(fname):
    cp = ConfigParser.SafeConfigParser()
    cp.read(fname)
    params = dict()
    for s in [ "default", "portrait" ]:
        for key, value in cp[s].items():
            params[key] = value
    return params

defparams = params.defparams
params = dict()
for p in defparams:
    if len(p) > 2:
        if len(p) == 4: p.append(False)
        (name, type, min_value, max_value, auto) = tuple(p)
        params[name] = dict(type = type, min = min_value, max = max_value, auto = auto)
    else:
        params[p[0]] = dict(type = p[1])

cfparams = get_params(os.path.join(os.path.dirname(__file__), '..', "okboard.cf"))
for name, value in cfparams.items():
    if name not in params: raise Exception("Unknown parameter in cf file: %s" % name)
    params[name]["value"] = params[name]["type"](value)

for p in params:
    if "value" not in params[p]:
        raise Exception("Parameter not in cf file: %s" % p)


cpus = multiprocessing.cpu_count()
pool = ThreadPool(processes = cpus)

def parallel(lst):
    global pool
    handle = dict()
    for key, func, args in lst:
        handle[key] = pool.apply_async(func, args = tuple(args))
    result = dict()
    for key, func, args in lst:
        result[key] = handle[key].get(timeout = 10)
    return result


def cleanup_detail(details):
    return " ".join([ "%s=%.2f" % (k, v) for (k, v) in details.items() ])

def dump_txt(txt, id = ""):
    sep = '--8<----[' + id + ']' + '-' * (65 - len(id))
    print(sep)
    print(txt)
    print(sep)
    print()

def log1(txt):
    if LOG is None: return
    with open(LOG, 'a') as f:
        f.write(txt)
        f.write("\n")

def run1(json_str, lang):
    opts = re.split('\s+', os.getenv('CLI_OPTS', ""))
    opts = [ x for x in opts if x ]
    cmd = [ CLI ] + opts + [ os.path.join(TRE_DIR, "%s.tre" % lang) ]

    sp = subprocess.Popen(cmd, stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = subprocess.PIPE)

    (json_out, err) = sp.communicate(bytes(json_str, "UTF-8"))
    return json_out, err, sp.returncode, ' '.join(cmd)

def update_json(json_str, params):
    js = json.loads(json_str)
    if "input" in js: js = js["input"]  # case we use an "output" json file
    js["params"] = dict([ (x, y["value"]) for x, y in params.items() ])
    return json.dumps(js)

def score1(json_str, expected, typ):
    # mot attendu premier = score > 0 (distance au prochain dans la liste)
    # sinon < 0 : somme des distance de ceux qui sont devant ! (x 10000)
    json_str = re.sub(r'(\n|\\n).*$', '', json_str)
    js = json.loads(json_str)

    score_ref = None
    others = []
    starred = False
    star_count = 0
    for c in js["candidates"]:
        score = c["score"]
        if score is None: raise Exception("null score for '%s'" % expected)
        name = c["name"]
        if name == expected:
            score_ref = score
            starred = c["star"]
        else:
            others.append(score)
            if c["star"]: star_count += 1

    # print "Detail:" + ",".join("%s: %.3f" % (c, c["score"]) for x in js["candidates"])

    if score_ref is None: return -9999999  # targeted word is not even found
    if not len(others): return 1  # sometime happens :-)

    average = sum(others) / len(others)
    stddev = math.sqrt(sum([(x - average) ** 2 for x in others]) / len(others))
    max_score = max(others)

    if typ == "stddev":
        score = score_ref - (average + stddev)

    elif typ == "cls":
        score = 0.5 * (score_ref - (average + stddev))
        if star_count > 0:
            if not starred: score -= 9999999
            else: score += 0.5 / (1.0 + star_count / 2)

    elif typ.startswith("max"):
        if score_ref >= max_score:
            score = score_ref - max_score
        else:
            coef = 10 if typ == "max2" else 1
            score = - coef * sum([ c["score"] - score_ref for c in js["candidates"] if c["score"] > score_ref ])

    else: raise Exception("unknown score type: %d", typ)

    return score

def save(fname, content):
    with open(fname, 'w') as f: f.write(content)

def run_all(tests, params, typ, fail_on_bad_score = False, return_dict = None, silent = False, dump = None):
    total, n = 0.0, 0
    runall = []
    inj = dict()
    wordk = dict()
    for (word, json_in, lang, test_id) in tests:
        key = test_id
        wordk[key] = word
        inj[word] = json_in
        log1("# %s (%s)" % (word, lang))
        sys.stdout.flush()
        json_in2 = update_json(json_in, params)
        log1(">>>> " + json_in2)
        runall.append((key, run1, [json_in2, lang ]))
        if dump:
            save(os.path.join(dump, "%s.in" % key), json_in2)

    results = parallel(runall)

    for key, result in results.items():
        (json_out, err, code, cmd) = result
        word = wordk[key]
        json_out, err = json_out.decode(encoding='UTF-8'), err.decode(encoding='UTF-8')

        if dump:
            save(os.path.join(dump, "%s.out" % key), json_out)
            save(os.path.join(dump, "%s.err" % key), err)
            save(os.path.join(dump, "%s.run" % key), cmd + '\n')

        if code != 0:
            dump_txt(inj[word])
            raise Exception("return code: %s [id=%s, word=%s, lang=%s] command: %s" % (code, key, word, lang, cmd))
        log1("<<<< " + json_out)
        log1("")

        i = json_out.find('Result: {')
        if i > -1: json_out = json_out[i + 8:]
        score = score1(json_out, word, typ)

        if not silent:
            print("%s (%s): " % (word, lang), "%.3f - " % score, end = "")
        if score < -999999 and fail_on_bad_score:
            dump_txt(json_out, "out")
            dump_txt(err, "err")
            dump_txt(json_in2, "in")
            raise Exception("negative score in reference test data: %s (type=%s, score=%d)" % (word, typ, score))
        total += score
        n += 1
        if return_dict is not None:
            return_dict[key] = score
    if not silent: print("OK")
    return total / n

def optim(pname, params, tests, typ):
    value = value0 = params[pname]["value"]
    min, max, type = params[pname]["min"], params[pname]["max"], params[pname]["type"]
    score0 = run_all(tests, params, typ)
    print("Optim: %s (value=%s, score=%s)" % (pname, value0, score0))

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
            print("Try[%s] %s->%s score=%f" % (pname, value, new_value, score))

            last_score = score
            if step < (max - min) / 10: step *= 2
            elif type == int: step += int(1 + (max - min) / 10)
            else: step *= 1.2

    value, score = sorted(scores, key = lambda x: x[1])[-1]
    if score > score0 + 0.01 * abs(score0):
        params[pname]["value"] = value
        print("===> Update[%s] %s->%s (score: %.3f -> %.3f)" % (pname, value0, value, score0, score))
        return score
    else:
        print("===> No change [%s]" % pname)
        params[pname]["value"] = value0
        return score0

def params2str(p):
    if not p: return "*none*"
    return ' '.join([ "%s=%s" % (x, y["value"]) for x, y in sorted(p.items()) ])

def load_tests():
    tests = []
    l = os.listdir(TEST_DIR)
    for fname in [ x for x in l if x[-5:] == '.json']:
        lang = "en"
        test_id = letters = fname[:-5]  # remove ".json"

        mo = re.match('^([a-z][a-z])\-(.*)', letters)
        if mo: lang, letters = mo.group(1), mo.group(2)

        letters = ''.join(c for c in unicodedata.normalize('NFD', letters) if unicodedata.category(c) != 'Mn')
        letters = re.sub(r'[^a-z]', '', letters.lower())
        letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
        tests.append((letters, open(os.path.join(TEST_DIR, fname)).read(), lang, test_id))

    tests.sort(key = lambda x: x[0])
    return tests

def run_optim(params, typ, listpara, p_include, p_exclude):
    params0 = copy.deepcopy(params)

    # load test suite
    tests = load_tests()

    print("===== Reference run =====")
    detail0 = dict()
    max_score = score = score0 = run_all(tests, params, typ, fail_on_bad_score = True, return_dict = detail0)
    max_params = copy.deepcopy(params)
    print()

    changed = True
    it = 0
    while changed:
        it += 1
        changed = False
        for p in params:
            if "min" not in params[p]: continue
            if listpara and p not in listpara: continue

            if p_include and not re.match(p_include, p): continue
            if p_exclude and re.match(p_exclude, p): continue
            # why ? if not params[p]["auto"] and not p_include: continue

            print("===== Optimizing parameter '%s' =====" % p)
            score = optim(p, params, tests, typ)
            if score > max_score:
                max_score = score
                max_params = copy.deepcopy(params)
                changed = True

            print("Start  (%.3f): %s" % (score0, params2str(params0)))
            print("Current(%.3f): %s" % (score, params2str(params)))
            print("Best   (%.3f): %s" % (max_score, params2str(max_params)))

    result_txt = '\n'.join([ "Parameter change: %s: %.3f -> %.3f" % (p, params0[p]["value"], v["value"])
                                   for p, v in sorted(max_params.items())
                                   if params0[p] != v ] + [ "Score: %.3f -> %.3f" % (score0, max_score) ])

    with open(OUT, "a") as f:
        detail_max = dict()
        run_all(tests, params, typ, return_dict = detail_max)
        fcntl.flock(f, fcntl.LOCK_EX)
        f.write('=' * 70 + '\n')
        f.write("Score type: %s - Time: [%s] - Duration: %d - Iterations: %d\n\n" % (typ, time.ctime(), int(time.time() - start), it))
        f.write("Start  (%.3f): %s\n" % (score0, params2str(params0)))
        f.write(" -> %s\n\n" % cleanup_detail(detail0))
        f.write("Best   (%.3f): %s\n" % (max_score, params2str(max_params)))
        f.write(" -> %s\n\n" % cleanup_detail(detail_max))
        f.write("%s\n\n" % result_txt)

    return result_txt

if __name__ == "__main__":
    start = time.time()

    p_include = p_exclude = None

    opts, args =  getopt.getopt(sys.argv[1:], 'l:p:i:x:')
    listpara = None
    for o, a in opts:
        if o == "-l":
            LOG = a
        elif o == "-p":
            listpara = a.split(',')
        elif o == "-i":
            p_include = a
        elif o == "-x":
            p_exclude = a
        else:
            print("Bad option: %s", o)
            exit(1)

    if len(args) < 1:
        print("usage:", os.path.basename(sys.argv[0]), " [<options>] <score type>")
        print("options:")
        print(" -p <list> : only optimize this parameters (comma separated)")
        print(" -l <file> : verbose log file")
        print(" -i <regexp> : only update parameters with matching names")
        print(" -x <regexp> : don't update parameters with matching names")
        exit(1)

    typ = args[0]

    os.chdir(os.path.dirname(sys.argv[0]))
    print('Parameters: ' + params2str(params))

    if typ == "all":
        typ_list = [ "max", "max2", "stddev", "cls" ]
    else:
        typ_list = [ typ ]

    results = dict()
    for typ in typ_list:
        result = run_optim(copy.deepcopy(params), typ, listpara, p_include, p_exclude)
        results[typ] = result

    for typ, result in results.items():
        print("===== %s =====" % typ)
        print(result)
        print()
