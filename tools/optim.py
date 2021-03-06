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
import pickle
import tempfile

CLI = "../cli/build/cli"
TRE_DIR = "../db"
TEST_DIR = "../test"
LOG = None  # "/tmp/optim.log"
OUT = "optim.log"
FAIL_SCORE = -999
EPS = 0.0002

def word2letters(word):
    letters = ''.join(c for c in unicodedata.normalize('NFD', word) if unicodedata.category(c) != 'Mn')
    letters = re.sub(r'[^a-z]', '', letters.lower())
    letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
    return letters

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
    params[name]["value"] = params[name]["default_value"] = params[name]["type"](value)

params["max_candidates"]["value"] = 50

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
        try:
            result[key] = handle[key].get(timeout = 60)
        except Exception as e:
            print("Pool error (key=%s)" % key)
            raise e
    return result


def cleanup_detail(details):
    return " ".join([ "%s=%.2f" % (k, v) for (k, v) in details.items() ])

def dump_txt(txt, id):
    dump_file = tempfile.NamedTemporaryFile(delete = False,
                                            prefix = os.path.join(tempfile.gettempdir(), "optim-%s-" % id),
                                            suffix = ".txt")
    dump_file.write(txt.encode('utf-8'))
    dump_file.close()
    print()
    print("*** Error Dump [%s] in file %s" % (id, dump_file.name))

def log1(txt):
    if LOG is None: return
    with open(LOG, 'a') as f:
        f.write(txt)
        f.write("\n")

def run1(json_str, lang, expected = None, nodebug = False, full = False):
    opts = re.split('\s+', os.getenv('CLI_OPTS', "-a 1"))  # by default test with incremental mode (more representative)
    opts = [ x for x in opts if x ]
    if nodebug: opts = [ "-g" ] + opts
    if expected: opts.extend([ "-e", expected ])
    cmd = [ CLI ] + opts + [ "-s", os.path.join(TRE_DIR, "%s%s.tre" % (lang, "-full" if full else "")) ]

    sp = subprocess.Popen(cmd, stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = subprocess.PIPE)

    (out, err) = sp.communicate(bytes(json_str, "UTF-8"))
    return out, err, sp.returncode, ' '.join(cmd)

def update_json(json_str, params):
    js = json.loads(json_str)
    if "input" in js: js = js["input"]  # case we use an "output" json file
    js["params"] = dict([ (x, y["value"]) for x, y in params.items() ])
    return json.dumps(js)

def score1(candidates, expected, typ):
    # mot attendu premier = score > 0 (distance au prochain dans la liste)
    # sinon < 0 : somme des distance de ceux qui sont devant ! (x 10000)

    score_ref = None
    others = []
    starred = False
    star_count = 0
    for c in candidates:
        score = candidates[c]
        if score is None: raise Exception("null score for '%s'" % c)
        if c == expected:
            score_ref = score
        else:
            others.append(score)

    if score_ref is None: return FAIL_SCORE  # targeted word is not even found
    if not len(others): return 1  # sometime happens :-)

    average = sum(others) / len(others)
    stddev = math.sqrt(sum([(x - average) ** 2 for x in others]) / len(others))
    max_score = max(others)

    if typ == "stddev":
        score = score_ref - (average + stddev)

    elif typ == "cls":
        score = 0.5 * (score_ref - (average + stddev))
        if star_count > 0:
            if not starred: score += FAIL_SCORE
            else: score += 0.5 / (1.0 + star_count / 2)

    elif typ.startswith("max"):
        if score_ref >= max_score:
            score = score_ref - max_score
        else:
            coef = 10 if typ.startswith("max2") else 1
            score = - coef * sum([ candidates[c] - score_ref for c in candidates if candidates[c] > score_ref ])
            if typ.endswith("b"):
                if max_score - score_ref > 0.01: score -= 1  # predict can not save it

    elif typ == "good":
        x = (score_ref - max_score) * 10
        score = x - x ** 2 if x < 0 else x / (1 + x)

    elif typ == "tanh":
        x = (score_ref - max_score) * 20
        score = math.tanh(x)

    elif typ == "tanh-1":
        x = ((score_ref - max_score) * 10 + 1)
        score = math.tanh(x * 20)

    elif typ == "good2":
        x = (score_ref - max_score) * 10
        score = 1 - x ** 4 if x < 0 else 0

    elif typ == "guess":
        score = 1 if score_ref >= max_score else 0

    elif typ == "plop":
        x = score_ref - max_score
        x2 = max(0., (0.01 - x) / 0.02)
        score = 1 - 4 * (x2 ** 2)

    elif typ == "plop2":
        x = score_ref - max_score
        score = 0
        if x > 0: score = 1
        if x < -0.01: score = -5

    else: raise Exception("unknown score type: %d", typ)

    return score

def save(fname, content):
    with open(fname, 'w') as f: f.write(content)

def run_all(tests, params, typ, fail_on_bad_score = False, return_dict = None, silent = False, dump = None, nodebug = False):
    total, n, bad_count = 0.0, 0, 0
    total_cpu = 0
    runall = []
    inj = dict()
    wordk = dict()
    for (word, json_in, lang, test_id) in tests:
        key = test_id
        if key in wordk: raise Exception("oops duplicate key: %s" % key)
        wordk[key] = word
        log1("# %s (%s)" % (word, lang))
        sys.stdout.flush()
        json_in2 = update_json(json_in, params)
        log1(">>>> " + json_in2)
        inj[key] = json_in2
        runall.append((key, run1, [json_in2, lang, word, nodebug ]))
        if dump:
            save(os.path.join(dump, "%s.in" % key), json_in2)

    results = parallel(runall)

    for key, result in results.items():
        (out, err, code, cmd) = result
        word = wordk[key]
        out, err = out.decode(encoding='UTF-8'), err.decode(encoding='UTF-8')
        json_in = inj[key]

        test_id = key
        lang = [ x for x in tests if x[3] == test_id ][0][2]

        if dump:
            save(os.path.join(dump, "%s.json" % key), json_in)
            save(os.path.join(dump, "%s.out" % key), out)
            save(os.path.join(dump, "%s.err" % key), err)
            save(os.path.join(dump, "%s.run" % key), cmd + '\n')
            save(os.path.join(dump, "%s.word" % key), word)

        if code != 0:
            print("\n*** Test failed: %s" % cmd)
            dump_txt(json_in, "in")
            dump_txt(out, "out")
            dump_txt(err, "err")
            raise Exception("return code: %s [id=%s, word=%s, lang=%s] command: %s" % (code, key, word, lang, cmd))
        log1("<<<< " + out)
        log1("")

        mo = re.search(r'cputime=(\d+)', err)
        cputime = int(mo.group(1)) if mo else -1

        candidates = dict()
        for li in out.split('\n'):
            li = li.strip()
            if not li: continue
            mo = re.match(r'^([\w\'\-]+)\s+([\d\-\.]+)$', li)
            candidates[word2letters(mo.group(1))] = float(mo.group(2))

        score = score1(candidates, word, typ)

        if not silent:
            print("%s (%s): " % (word, lang), "%.3f - " % score, end = "")
        if score <= FAIL_SCORE:
            if fail_on_bad_score:
                print("\n*** Test failed: %s (%s)" % (cmd, test_id))
                dump_txt(json_in, "in")
                dump_txt(out, "out")
                dump_txt(err, "err")
                raise Exception("negative score in reference test data: %s (id=%s, type=%s, score=%d, lang=%s)" % (word, test_id, typ, score, lang))
            else:
                bad_count += 1
        total += score
        total_cpu += cputime
        n += 1
        if return_dict is not None:
            return_dict[key] = score
    if not silent: print("OK")
    return total / n, total_cpu / n, (n - bad_count) / n

def optim(pname, params, tests, typ):
    value = value0 = params[pname]["value"]
    min, max, type = params[pname]["min"], params[pname]["max"], params[pname]["type"]
    score0, _ignored_, _ig2_ = run_all(tests, params, typ, nodebug = True)
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
            score, _ignored_, _ig2_ = run_all(tests, params, typ, nodebug = True)
            if score > last_score: bad_count = 0
            else: bad_count += 1

            if bad_count > 6: break

            scores.append( (new_value, score) )
            print("Try[%s:%s] %s->%s score=%f" % (typ, pname, value, new_value, score))

            last_score = score
            if step < (max - min) / 10: step *= 2
            elif type == int: step += int(1 + (max - min) / 10)
            else: step *= 1.2

    value, score = sorted(scores, key = lambda x: x[1])[-1]
    if score > score0 + EPS:
        params[pname]["value"] = value
        print("===> Update[%s:%s] %s->%s (score: %.3f -> %.3f)" % (typ, pname, value0, value, score0, score))
        return score
    else:
        print("===> No change [%s:%s]" % (typ, pname))
        params[pname]["value"] = value0
        return score0

def params2str(p):
    if not p: return "*none*"
    return ' '.join([ "%s=%s" % (x, y["value"]) for x, y in sorted(p.items()) ])

def load_tests(test_dir = None):
    tests = []
    if not test_dir: test_dir = TEST_DIR
    elif isinstance(test_dir, list):
        for td in test_dir: tests.extend(load_tests(td))
        return tests

    l = os.listdir(test_dir)
    for fname in [ x for x in l if x[-5:] == '.json']:
        lang = "en"
        test_id = letters = fname[:-5]  # remove ".json"

        mo = re.match('^([a-z][a-z])\-(.*)', letters)
        if mo:
            if os.path.isfile(os.path.join(TRE_DIR, "%s.tre" % mo.group(1))):
                lang, letters = mo.group(1), mo.group(2)

        letters = re.sub(r'-.*$', '', letters)
        letters = word2letters(letters)
        tests.append((letters, open(os.path.join(test_dir, fname)).read(), lang, test_id))

    tests.sort(key = lambda x: x[0])
    return tests

def run_optim_one_shot(params, typ, listpara, p_include, p_exclude, param_file, test_dir = None, fail_on_bad_score = True):
    params0 = copy.deepcopy(params)
    tests = load_tests(test_dir)
    score0, _ignored_, _ig2_ = run_all(tests, params, typ, fail_on_bad_score = fail_on_bad_score, nodebug = True)
    result_txt = ""

    for p in sorted(list(params.keys())):
        if "min" not in params[p]: continue
        if listpara and p not in listpara: continue

        if p_include and not re.match(p_include, p): continue
        if p_exclude and re.match(p_exclude, p): continue

        params = copy.deepcopy(params0)

        print("===== Optimizing parameter '%s' =====" % p)
        last_score = score0
        while True:
            score = optim(p, params, tests, typ)
            if score <= last_score: break
            last_score = score
            changed = True

        if score > score0:
            bla = "%s: Parameter change[%s]: %.4f -> %.4f: score: %.4f -> %.4f" % \
                  (typ, p, params0[p]["value"], params[p]["value"], score0, score)
            print(">", bla)
            result_txt += bla + "\n"
            with open(OUT, "a") as f:
                f.write(bla + "\n")

    return result_txt

def run_optim(params, typ, listpara, p_include, p_exclude, param_file, test_dir = None, fail_on_bad_score = True):
    params0 = copy.deepcopy(params)

    # load test suite
    tests = load_tests(test_dir)

    # load saved parameters
    if param_file:
        saved_params = dict()
        try:
            with open(param_file, 'rb') as f:
                saved_params = pickle.load(f)
        except: pass  # bad or missing file
        if typ in saved_params:
            for p, v in saved_params[typ].items():
                print("Loading parameter value [%s]: %s = %s" % (typ, p, v[0]))
                params[p]["value"] = v[0]

    print("===== Reference run =====")
    detail0 = dict()
    score, _ignored_, _ig2_ = run_all(tests, params, typ, fail_on_bad_score = fail_on_bad_score, return_dict = detail0, nodebug = True)
    max_score = score0 = score
    max_params = copy.deepcopy(params)
    print()

    changed = True
    it = 0
    all_params = set()
    while changed:
        it += 1
        changed = False
        for p in params:
            if "min" not in params[p]: continue
            if listpara and p not in listpara: continue

            if p_include and not re.match(p_include, p): continue
            if p_exclude and re.match(p_exclude, p): continue
            # why ? if not params[p]["auto"] and not p_include: continue

            all_params.add(p)

            print("===== Optimizing parameter '%s' =====" % p)
            score = optim(p, params, tests, typ)
            if score > max_score:
                max_score = score
                max_params = copy.deepcopy(params)
                changed = True

            print("Start  (%.3f): %s" % (score0, params2str(params0)))
            print("Current(%.3f): %s" % (score, params2str(params)))
            print("Best   (%.3f): %s" % (max_score, params2str(max_params)))

            # save parameters
            if param_file:
                if typ not in saved_params: saved_params[typ] = dict()
                if params[p]["value"] != params[p]["default_value"]:
                    saved_params[typ][p] = (params[p]["value"], params[p]["default_value"])
                    with open(param_file + '.tmp', 'wb') as f:
                        pickle.dump(saved_params, f)
                    os.rename(param_file + '.tmp', param_file)

    result_txt = '\n'.join([ "Parameter change[%s]: %s: %.3f -> %.3f" % (typ, p, params0[p]["value"], v["value"])
                                   for p, v in sorted(max_params.items())
                                   if params0[p] != v ] + [ "Score: %.3f -> %.3f" % (score0, max_score) ])

    if len(all_params) == 1:
        p = all_params.pop()
        print("=update %s:%s %.3f -> %.3f [%4f] %s" %
              (typ, p, params0[p]["value"], max_params[p]["value"],
               max_score - score0, "OK" if params0[p] != max_params[p] else "-"))

    with open(OUT, "a") as f:
        detail_max = dict()
        run_all(tests, params, typ, return_dict = detail_max, nodebug = True)
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
    param_file = None
    one_shot = False
    test_dir = None
    fail_on_bad_score = True
    usage = False

    opts, args =  getopt.getopt(sys.argv[1:], 'l:p:i:x:s:oT:fh')
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
        elif o == "-s":
            param_file = os.path.realpath(a)
        elif o == "-o":
            one_shot = True
        elif o == "-T":
            test_dir = [ os.path.realpath(x) for x in a.split(',') ]
        elif o == "-f":
            fail_on_bad_score = False
        elif o == "-h":
            usage = True
        else:
            print("Bad option: %s", o)
            exit(1)

    if len(args) < 1 or usage:
        print("usage:", os.path.basename(sys.argv[0]), " [<options>] <score type>")
        print("options:")
        print(" -p <list> : only optimize this parameters (comma separated)")
        print(" -l <file> : verbose log file")
        print(" -i <regexp> : only update parameters with matching names")
        print(" -x <regexp> : don't update parameters with matching names")
        print(" -s <file> : save parameters to file (and read them on startup)")
        print(" -o : one-shot (optimize all parameters separately")
        print(" -T <dirs> : read test files from these directories (comma separated)")
        print(" -f : optimize even if some tests are failing")
        print("score type can be 'all' or a comma separated list")
        exit(1)

    typ = args[0]

    os.chdir(os.path.dirname(sys.argv[0]))
    print('Parameters: ' + params2str(params))

    if typ == "all":
        typ_list = ["max", "max2", "max2b", "guess" ]
    elif typ.find(',') > -1:
        typ_list = typ.split(',')
    else:
        typ_list = [ typ ]

    with open(OUT, "a") as f:
        f.write("\n=== optim " + time.ctime(time.time()) + " / " + " " . join(sys.argv[1:]) + " ===\n\n")

    results = dict()
    for typ in typ_list:
        optim_f = run_optim_one_shot if one_shot else run_optim
        result = optim_f(copy.deepcopy(params), typ, listpara, p_include, p_exclude, param_file,
                         test_dir = test_dir, fail_on_bad_score = fail_on_bad_score)
        results[typ] = result

    for typ, result in results.items():
        print("===== %s =====" % typ)
        print(result)
        print()
