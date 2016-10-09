#! /usr/bin/python3
# -*- coding: utf-8 -*-

# replay functional test suite from a pickled file

import sys, os, re
import pickle
import math
import getopt

mydir = os.path.dirname(os.path.abspath(__file__))
mydir = libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import language_model, backend

from dev_tools import Tools

def play_all(records, tools, backtrack = False, verbose = True, mock_time = False,
             learn = False, db_path = None, db_reset = True, display_stats = True,
             learn_replace = False):
    all_langs = set([ x["lang"] for x in records ])
    count = count_ok = count_bad = 0
    bt_count = bt_ok = 0

    if learn and not db_path: raise Exception("Learning can not be tested on development database")

    if not db_path: db_path = os.path.join(mydir, "db")

    print("# %d records - %d langs" % (len(records), len(all_langs)))
    for lang in sorted(all_langs):
        lang_filter = tools.cf("lang", "", str)
        if lang_filter and lang != lang_filter: continue

        db_file = os.path.join(db_path, "predict-%s.db" % lang)
        db = backend.FslmCdbBackend(db_file)
        if db_reset: db.factory_reset()
        lm = language_model.LanguageModel(db, tools = tools, debug = False, dummy = False)

        guessed_context = []
        hist = []

        for t in records:
            if t["lang"] != lang: continue

            if mock_time:
                lm.mock_time(int(t["ts"]))

            tools.log("")
            details = dict()
            context = list(t["context"])
            learn_context = list(context)

            if not context or context[-1] == "#START":
                guessed_context = []

            guesses = lm.guess(context, t["candidates"],
                               correlation_id = t["id"], speed = t["speed"],
                               verbose = verbose, expected_test = t["expected"], details_out = details)

            # custom metrics example:
            avg = lambda l: sum(l) / len(l)
            score_curve = lambda w: avg(details[w]["curve"])
            exp = t["expected"]

            hist = [ (exp, guesses[0] if guesses else None) ] + hist[:10]

            if exp not in guesses:
                fields = [ "NOT_FOUND", -1, -1 ]
                count_bad += 1
            else:
                fields = [ "OK" if exp == guesses[0] else "FAIL",
                           "%.4f" % (max([ score_curve(g) - score_curve(exp) for g in guesses ])),
                           "%.4f" % (score_curve(guesses[0]) - score_curve(exp)) ]

            fields.extend([t["id"], lang, exp, guesses[0] if guesses else None,
                           "speed=%d:%d" % (t["speed"], t["speed_max"]),
                           "st=%d:%d" % (t["st1"], t["st2"]),
                           "=rpt" ])
            tools.log(" " . join([ str(x) for x in fields ]))

            # report C2/C3 scores
            def ratio(a, b):
                if a == 0 and b == 0: return 0
                if a == 0: return -100
                if b == 0: return 100
                return "%.2f" % math.log10(a / b)

            if exp in guesses:
                if exp == guesses[0]:
                    fields = [ "OK",
                               "(no-info)" ]
                else:
                    g = guesses[0]
                    fields = [ "FAIL",
                               t["id"], lang, exp, guesses[0],
                               "curve=%.3f" % (score_curve(exp) - score_curve(g)),
                               int("s1" in details[exp]["coefs"]) ] + \
                               [ "%s=%s" % (x.upper(), ratio(details[exp]["coefs"].get(x, 0), details[g]["coefs"].get(x, 0)))
                                 for x in [ "c3", "c2", "s3", "s2", "s1" ] ] + \
                               [ "rank=%d/%d" % (guesses.index(exp), len(guesses)) ]
                tools.log(" " . join([ str(x) for x in fields + [ "=rp2" ] ]))

            # end of reports

            ok = guesses and guesses[0] == exp
            count += 1
            if ok: count_ok += 1
            tools.log("[*] Guess %s %s \"%s\" (expected \"%s\"): %s" %
                      (t["id"], t["context"], guesses[0] if guesses else "?", t["expected"], ("=OK=" if ok else "*FAIL*")))

            if guesses: guessed_context.append(guesses[0])
            else: guessed_context =  []

            if learn:
                # @todo use right "replace" value when backtracking is active
                replaces = guesses[0] if not ok and guesses and learn_replace else None
                tools.log("[*] Learn %s:" % t["id"], learn_context, exp, ("{%s}" % replaces) if replaces else "")
                lm.learn(True, exp, list(reversed(learn_context)),
                         replaces = replaces)
                if count % 50 == 0: lm.cleanup(force_flush = True)

            if backtrack:
                tools.log("")
                rst = lm.backtrack(verbose = verbose, testing = True, learn = False) # or learn = learn ??
                if rst:
                    bt_count += 1
                    index, old, new, id = rst
                    if hist[index][1] != old: raise Exception("boum: " + hist[index][1] + "," + old)  # sanity check
                    old_ok = (hist[index][1] == hist[index][0])
                    new_ok = (new == hist[index][0])

                    if new_ok:
                        bt_ok += 1
                        st = "=BT-OK="
                        count_ok += 1
                    elif old_ok:
                        st = "*BT-FAIL*"
                        count_ok -= 1
                    else: st = "*BT-FAIL*"
                    tools.log("[*] Backtracking %s %s %s -> %s %s" % (t["id"], guessed_context, old, new, st))
                    # @todo learn

        if learn: lm.cleanup(force_flush = True)
        lm.close()
        db.close()

    if display_stats:
        print("Summary: total=%d OK=(%.2f%%) bad=(%.2f%%)" % (count, 100.0 * count_ok / count, 100.0 * count_bad / count))
        if bt_count: print("Summary: backtrack=%d OK=%d (%.2f%%)" % (bt_count, bt_ok, 100.0 * bt_ok / bt_count))

    return 100.0 * count_ok / count

def cli_params(args, tools):
    file_in = None
    filt = None
    for arg in args:
        mo = re.match(r'^([0-9a-z_]+)=(.*)$', arg)
        if mo:
            value = mo.group(2)
            if re.match(r'^[\d\.\-Ee]*$', value): value = float(value)
            tools.params[mo.group(1)] = value
        elif file_in:
            filt = arg
        elif os.path.exists(arg):
            file_in = arg
        else:
            raise Exception("bad arg: %s" % arg)
    if not file_in: raise Exception("Add pickle file name as argument !")
    records = pickle.load(open(file_in, 'rb'))
    if filt:
        records = [ r for r in records if r['id'] == filt ]
        if not records: raise Exception("Record not found: %s" % filt)
    return records

if __name__ == "__main__":
    learn = backtrack = db_reset = mock_time = learn_replace = last_verbose = all_verbose = False
    db_path = None
    repeat = 1

    def usage():
        print("Usage: %s [<options>] <ftest pickle log file> [<test id>]" % os.path.basename(__file__))
        print("Options:")
        print(" -l       : learn")
        print(" -b       : backtrack")
        print(" -r       : database reset (enables repeatable results)")
        print(" -m       : mock_time")
        print(" -c       : repeat count")
        print(" -p <dir> : use alternate database files")
        print(" -R       : learn correction of bad guess by user (negative learning)")
        print(" -V       : make last iteration verbose")
        print(" -v       : verbose mode for all iteration (beware of volume)")

    opts, args =  getopt.getopt(sys.argv[1:], 'lbrmp:c:hRVv')
    listpara = None
    for o, a in opts:
        if o == "-l": learn = True
        elif o == "-b": backtrack = True
        elif o == "-p": db_path = a
        elif o == "-r": db_reset = True
        elif o == "-m": mock_time = True
        elif o == "-c": repeat = int(a)
        elif o == "-R": learn = learn_replace = True
        elif o == "-V": last_verbose = True
        elif o == "-v": all_verbose = True
        elif o == "-h": usage(); exit(0)
        else: print("Bad option: %s", o); usage(); exit(1)

    tools = Tools()

    if not args: usage(); exit(1)
    records = cli_params(args, tools)

    tools.verbose = verbose = (repeat == 1) or all_verbose

    for i in range(repeat):
        if last_verbose and i == repeat - 1: tools.verbose = verbose = True
        if repeat > 1: print("Iteration #%d" % (i + 1))

        play_all(records, tools, learn = learn, backtrack = backtrack, db_reset = db_reset,
                 verbose = verbose, db_path = db_path, mock_time = mock_time,
                 learn_replace = learn_replace)
        db_reset = False  # only reset on first iteration

    print(tools.params)
