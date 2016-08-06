#! /usr/bin/python3
# -*- coding: utf-8 -*-


import sys, re, os
import json, time
import getopt
import pickle

mydir = os.path.dirname(os.path.abspath(__file__))
mydir = libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import language_model, backend

def obj2dict(o):
    d = dict()
    for a in dir(o):
        if a.startswith('_') or a == "copy": continue
        d[a] = getattr(o, a)
    return d

def remove_quotes(word):
    return  re.sub(r'^([\'\"])(.*)\1$', r'\2', word)

class Tools:
    def __init__(self, params = dict(), verbose = True):
        self.messages = []
        self.params = params
        self.verbose = verbose

    def log(self, *args, **kwargs):
        if not args: return
        message = ' '.join(map(str, args))
        if self.verbose: print(message)
        self.messages.append(message)

    def cf(self, key, default_value, cast):
        return cast(self.params.get(key, default_value))

def ftest_load(index_org, work_dir, params = dict()):
    current = None
    history = []

    for line in sys.stdin.readlines():
        line = line.strip()

        if line.startswith("==WORD=="):
            if current:
                # handle incomplete predict logs (due to temporary regression)
                current["word"] = current["guessed"] = "XXX"
                if "js" in current: history.append(current)

            (dummy, id, word, device_id, date) = line.split(' ', 4)
            ts = int(id.split('-')[0])
            word = remove_quotes(word)
            current = dict(id = id, ts = ts, word = word, device_id = device_id, date = date, content = "", js = None)
            continue

        if not current: continue

        if line.startswith('OUT:'):
            js = line[4:].strip()
            current["js"] = js
            continue

        mo = re.search(r'^ID:.*Context:\s*\[([^\[\]]*)\]', line)
        if mo:
            context = current["context"] = list(reversed([ remove_quotes(x.strip()) for x in mo.group(1).split(',') ]))
            continue

        mo = re.search(r'^Selected word:\s*(\w+)', line)
        if mo:
            current["word"] = current["guessed"] = mo.group(1)
            if "js" in current: history.append(current)
            current = None

    history.sort(key = lambda x: x["id"], reverse = False)

    # read org file
    db = dict()
    if os.path.exists(index_org):
        with open(index_org, 'r') as f:
            for li in f.readlines():
                li = li.strip()
                mo = re.match(r'^\s*\-\ \[(.)\]\s+([0-9\-]+)', li)
                if not mo: continue
                check, id = mo.group(1), mo.group(2)
                li = re.sub(r'\s*\-\>.*$', '', li)
                li = re.sub(r'^.*\]\s*', '', li)
                context = list(re.split(r'\s+', li))
                word = context.pop(-1)
                db[id] = (check == 'X'), word, context

    return history, db

if __name__ == '__main__':

    add_new = False
    quick = False
    opts, args =  getopt.getopt(sys.argv[1:], 'aq')
    for o, a in opts:
        if o == "-a": add_new = True
        elif o == "-q": quick = True
        else:
            print("Bad option: %s" % o)
            exit(1)

    if len(args) < 2:
        print("usage:", os.path.basename(__file__), "[-a] <in/out org index file> <work directory> [<option key=value> ... ]")
        print(" (it reads logs from stdin)")
        exit(1)

    index_org, work_dir = args[0:2]
    params = dict( [ x.split('=', 1) for x in args[2:] ] )

    history, db = ftest_load(index_org, work_dir, params)

    # write org file
    f = open(index_org + ".tmp", 'w')
    f.write("# OKBoard replay\n\n[%s] Parameters: %s\n\n" % (time.ctime(), params))
    last_date = None
    count = ok_count = found_count = badcurve_count = missing_count = 0
    last_lang = None
    lm = None

    tools = Tools(params, verbose = not quick)

    record = []
    manifest = []

    for t in history:
        id = t["id"]
        ts = t["ts"]
        word = t["word"]
        context = t["context"]
        pre = os.path.join(work_dir, id)

        load_ok = False
        lang = None
        if quick:
            if os.path.isfile(pre + ".txt"):
                candidates = dict()
                for li in open(pre + ".txt", "r").readlines():
                    li = li.strip()
                    if not li: continue
                    word, score = li.split(" ")
                    candidates[word] = float(score)
                err = open(pre + ".err", "r").read()
                mo = re.search(r'speed=(\d+)', err)
                speed = int(mo.group(1)) if mo else -1
                mo = re.search(r'treefile=([a-z0-9A-Z\/\.\-\_]+)', err)
                if mo: lang = os.path.basename(mo.group(1))[:2]
                load_ok = True
                speed_max = -1

        else:
            if os.path.isfile(pre + ".json"):
                result = json.load(open(pre + ".json", 'r'))
                speed = result["stats"]["speed"]
                candidates = language_model.split_old_candidate_list(result["candidates"])
                lang = os.path.basename(result["input"]["treefile"])[:2]
                speed_max = max([ c.get("speed", 0) for c in result["input"]["curve"] ])
                load_ok = True

        check = False
        comment = False
        if id in db:
            check, word, context = db[id]
        elif not add_new:
            comment = True

        if lang and lang != last_lang:
            if lm: lm.close()
            db_file = os.path.join(mydir, "db/predict-%s.db" % lang)
            print("Language change: %s -> %s" % (last_lang or "None", lang))
            if os.path.exists(db_file):
                lang_db = backend.FslmCdbBackend(db_file)
                lm = language_model.LanguageModel(lang_db, tools = tools)
            else:
                # oops maybe we have tried third parties language files and user a language
                # not in the standard distribution
                lm = None
            last_lang = lang

        prefix = "  - [%s] %s %s [[file:%s][json]] [[file:%s][log]] [[file:%s][html]] [[file:%s][png]] [[file:%s][predict log]] %s %s" % \
                 ("X" if check else " ", id, lang, pre + ".json", pre + ".log", pre + ".html", pre + ".png", pre + ".predict.log",
                  " ".join(context), word)

        if comment: prefix = "  # use -a option to add: " + prefix.strip()

        date = time.strftime("%Y-%m-%d %a", time.localtime(ts))
        if last_date != date:
            last_date = date
            f.write("* <%s>\n" % date)

        if not check or comment:
            f.write("%s\n" % prefix)
            continue

        if not lang or not lm: continue
        if not load_ok: raise Exception("Could not load file: %s.json" % pre)


        print("==== %s [%s] %s (lang=%s)" % (id, " ".join(context), word, lang))
        print("candidates:", ','.join(candidates.keys()))

        details = dict()
        ordered_guesses = lm.guess(context, candidates,
                                   correlation_id = id, speed = speed,
                                   verbose = True, expected_test = word, details_out = details)

        guess = ordered_guesses[0] if ordered_guesses else None

        ok = (guess == word)
        count += 1
        if ok: ok_count += 1

        compare_word = guess
        if len(ordered_guesses) < 2: compare_word = None
        elif ok: compare_word = ordered_guesses[1]

        if quick:
            st1 = st2 = -1
        else:
            curve = result["input"]["curve"]
            st1 = len([ 1 for c in curve if c.get("sharp_turn", 0) == 1 ])
            st2 = len([ 1 for c in curve if c.get("sharp_turn", 0) == 2 ])

        record.append(dict(id = id, ts = ts, context = context, candidates = candidates,
                           expected = word, lang = lang, speed = speed, old_guess = guess,
                           compare_word = compare_word,
                           speed_max = speed_max,
                           st1 = st1, st2 = st2))

        if ok: rank = 0
        else:
            try: rank = ordered_guesses.index(word)
            except: rank = -1

        if rank >= 0: found_count += 1

        if ok: st = "=OK="
        elif rank > 0: st = "*FAIL* (%s:%d)" % (ordered_guesses[0], rank)
        else: st = "*FAIL* NOT-FOUND"

        if not candidates:
            gap = 0
        elif word not in candidates:
            gap = 1
        else:
            gap = max(list(candidates.values())) - candidates[word]

        if gap == 1:
            st += " #NOTFOUND"
            badcurve_count += 1
        elif rank < 0 or gap > 0.05:
            st += " #BADCURVE"
            badcurve_count += 1

        print("> Result [%s]: %s (%s) gap=%.2f rank=%d" % (id, st,
                                                           word if ok else ("%s/%s" % (guess, word)),
                                                           gap, rank))

        manifest.append(';'.join([ str(x) for x in [ id, word, guess, gap, rank, st, lang ] ]))

        # @todo add wiring to learning / backtracking

        with open(pre + ".predict.log", 'w') as pf:
            pf.write("Summary: %s %s %s %s\n" % (lang, word, guess, ok))
            pf.write("\n".join(tools.messages))
        tools.messages = []

        f.write("%s -> %s\n" % (prefix, st))

    if lm: lm.close()

    stats = "Total=%d OK=%d rate=%.2f%% found_rate=%.2f%% bad_curve=%.2f%% missing=%d" % (count, ok_count, 100.0 * ok_count / count,
                                                                                          100.0 * found_count / count, 100.0 * badcurve_count / count,
                                                                                          missing_count)
    print(stats)

    log_file = os.path.join(os.path.dirname(index_org), "ftest.log")
    with open(log_file, 'a') as lf:
        lf.write("[%s] %s %s\n" % (time.ctime(), stats, params))

    f.write("""
* stats
  %s

* org-mode configuration
  :PROPERTIES:
  :VISIBILITY: folded
  :END:
#+STARTUP: showall
""" % stats)

    f.close()
    if quick:
        os.unlink(index_org + ".tmp")
    else:
        os.rename(index_org + ".tmp", index_org)

    pck_file = os.path.join(work_dir, "ftest-all.pck")
    with open(pck_file, 'wb') as f:
        pickle.dump(record, f)

    mf_file = os.path.join(work_dir, "manifest.txt")
    with open(mf_file, 'w') as f:
        f.write('\n'.join(manifest) + '\n')
