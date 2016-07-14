#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os
import pickle
import json
import unicodedata
import re
import glob

dir = sys.argv[1]
try:
    # end-to-end tests directory
    records = pickle.load(open(os.path.join(dir, "ftest-all.pck"), 'rb'))
    extension = "json"
except:
    # simple curve test dump directory (from test.py script)
    records = []
    for fn in glob.glob(dir + "/*.word"):
        records.append(dict(id = os.path.basename(fn)[:-5],
                            expected = open(fn, 'r').read()))
    extension = "out"

def word2letters(word):
    letters = ''.join(c for c in unicodedata.normalize('NFD', word.lower()) if unicodedata.category(c) != 'Mn')
    letters = re.sub(r'[^a-z]', '', letters.lower())
    letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
    return letters

def get_detail(js, word):
    word = word2letters(word)
    return [ x for x in js["candidates"] if x["name"] == word ][0]

def parse_misc_acct(js):
    ret = dict()
    for s in js["scenarios"]:
        if "misc_acct" not in s: continue
        for x in s["misc_acct"]:
            name = x["coef_name"]
            if name not in ret: ret[name] = 0
            ret[name] += x["coef_value"] * x["value"]
    ret["misc"] = js["avg_score"]["score_misc"]
    return ret

def avg(l): return 1.0 * sum(l) / len(l)

total = dict()

def add_total(key, value, id = None):
    global total
    if key not in total: total[key] = dict(total = 0.0, count = 0, plus = 0, minus = 0, total_bad = 0, avg_bad = 0, bad = [])
    total[key]["total"] += value
    total[key]["count"] += 1
    if value > 0: total[key]["plus"] += 1
    if value < 0:
        total[key]["minus"] += 1
        total[key]["total_bad"] += value
        if id: total[key]["bad"].append(id)
    total[key]["avg"] = total[key]["total"] / total[key]["count"]
    total[key]["avg_bad"] = total[key]["total_bad"] / total[key]["minus"] if total[key]["minus"] else 0

for t in records:
    id = t["id"]
    exp = t["expected"]
    cmp = t.get("compare_word", None)

    content = open(os.path.join(dir, "%s.%s" % (id, extension)), 'r').read()
    if content[0] != '{':
        content = content[content.index('{'):]
    js = json.loads(content)
    candidates = [ c["name"] for c in js["candidates"] ]
    if len(candidates) <= 1: continue
    if exp not in candidates: continue

    if not cmp:
        candidates.sort(key = lambda c: [ x["score"] for x in js["candidates"] if x["name"] == c ][0], reverse = True)
        if exp == candidates[0]: cmp = candidates[1]
        else: cmp = candidates[0]



    exp_v = parse_misc_acct(get_detail(js, exp))
    cmp_v = parse_misc_acct(get_detail(js, cmp))

    keys = set(exp_v.keys())

    all = dict()
    for c in candidates:
        if c == exp: continue
        other = parse_misc_acct(get_detail(js, c))
        keys = keys.union(other.keys())

        for k in other.keys():
            if k not in all: all[k] = []
            all[k].append(other.get(k, 0))

    bad = set()
    for k in keys:
        exp_value = exp_v.get(k, 0)
        add_total("%s_max" % k, exp_v.get(k, 0) - max(all.get(k, [0])))
        add_total("%s_avg" % k, exp_v.get(k, 0) - avg(all.get(k, [0])))
        add_total("%s_cmp" % k, exp_v.get(k, 0) - cmp_v.get(k, 0))

        if exp_v.get(k, 0) < cmp_v.get(k, 0):
            bad.add(k)

    if bad:
        print("%s [%s]: %s" % (exp, id, ", ".join(bad)))

for k in sorted(total.keys()):
    print("%s: avg=%.4f count=%d +/-=[%d/%d] bad=%.4f avg_bad=%.4f %s" %
          (k, total[k]["avg"], total[k]["count"], total[k]["plus"], total[k]["minus"],
           total[k]["total_bad"], total[k]["avg_bad"], ",".join(total[k]["bad"])))
