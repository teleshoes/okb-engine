#! /usr/bin/python3

import os, sys
import pickle
import json
import re

manifest = sys.argv[1]

comment = dict()
if len(sys.argv) >= 3:
    org_file = sys.argv[2]
    try:
        with open(org_file, "r") as f:
            for li in f.readlines():
                mo = re.search(r'^\s+\-\s+(\S+).*\ :\s+(.*)', li.rstrip())
                if mo and mo.group(1): comment[mo.group(1)] = mo.group(2)
                else:
                    mo2 = re.search(r'^\s+\-\s+comment\s+(\S+)\s*:\s*(.*)', li.rstrip())
                    if mo2 and mo2.group(1): comment[mo.group(1)] = mo.group(2)
    except: pass
    out = open(org_file + ".tmp", "w")
else:
    out = sys.stdout
    org_file = None
records = pickle.load(open(manifest, 'rb'))
dir = os.path.dirname(manifest)

flags = dict()
items = dict()

out.write("# failure cause analysis\n\n* test cases\n")

def flag(id, value):
    global flags, item
    if value not in flags: flags[value] = 0
    flags[value] += 1
    if id not in items: items[id] = set()
    items[id].add(value)


count = 0
fail_count = 0
for r in records:
    id = r['id']
    lang = r['lang']
    expected, guessed = r['expected'], r['old_guess']

    count += 1
    if not count % 100: sys.stderr.write("%d/%d\n" % (count, len(records)))

    # curve (json)
    json_file = os.path.join(dir, "%s.json" % id)
    js = json.load(open(json_file, 'r'))

    found = [ c for c in js["candidates"] if c["name"] == expected or expected in c["words"].split(',') ]
    if not found:
        flag(id, "not_found")
        continue
    sc_expected = found[0]

    # predict (txt)
    predict = dict()
    guess = None
    with open(os.path.join(dir, "%s.predict.log" % id), 'r') as f:
        for li in f.readlines():
            if li.startswith('-'):
                mo = re.search(r'^\-\s+([\d\.]+)\s+\[([^\[\]]+)\]([\s\*]+)(\S+)', li.strip())
                score, cause, star, word = float(mo.group(1)), mo.group(2).strip(), mo.group(3), mo.group(4)
                if star.find('*') > -1 and word != expected: raise Exception("oops")
                scenario = [ c for c in js["candidates"] if c["name"] == word or word in c["words"].split(',') ][0]
                curve_score = scenario["score"]
                predict[word] = dict(score = score, cause = cause,
                                     curve_score = curve_score, predict_score = score - curve_score)
                if not guess: guess = word

    if expected not in predict:
        flag(id, "predict_filtered")
        continue

    if guess == expected: continue  # guess OK
    fail_count += 1

    sc_curve_guess = [ c for c in js["candidates"] if c["name"] == guess or guess in c["words"].split(',') ][0]

    # curve flags
    if sc_curve_guess["name"] != sc_expected["name"]:
        flag(id, "bad_curve_guess")
        sg, se = sc_curve_guess, sc_expected
        if se["avg_score"]["score_turn"] < sg["avg_score"]["score_turn"]: flag(id, "score_turn")
        if se["avg_score"]["score_turn"] < sg["avg_score"]["score_turn"] - 0.005: flag(id, "score_turn_BAD")
        if se["avg_score"]["score_misc"] < sg["avg_score"]["score_misc"]: flag(id, "score_misc")
        if se["avg_score"]["score_misc"] < sg["avg_score"]["score_misc"] - 0.005: flag(id, "score_misc_BAD")

        if se["new_dist"] > sg["new_dist"] * 1.1 or se["new_dist"] > sg["new_dist"] + 5: flag(id, "newdist")
        if se["new_dist"] < sg["new_dist"] * 0.8 or se["new_dist"] < sg["new_dist"] - 15: flag(id, "(newdist good)")

        def misc_score(scenario):
            ret = dict()
            for s in scenario["scenarios"]:
                if "misc_acct" not in s: return dict()
                for sc in s["misc_acct"]:
                    key = "misc:" + sc["coef_name"]
                    if key not in ret: ret[key] = 0
                    ret[key] += sc["coef_value"] * sc["value"]
            return ret

        me, mg = misc_score(sc_expected), misc_score(sc_curve_guess)

        keys = set(me.keys()).union(set(mg.keys()))
        for k in keys:
            diff = me.get(k, 0) - mg.get(k, 0)
            if diff < 0: flag(id, k)
            if diff < -0.005: flag(id, "%s_BAD" % k)

    # predict flags
    pe = predict[expected]["predict_score"]
    pg = predict[guess]["predict_score"]
    if pe < pg: flag(id, "predict")
    if pe < pg - 0.002: flag(id, "predict_BAD")
    if pe > pg - 0.002: flag(id, "(predict good)")
    if predict[expected]["cause"].startswith("C:"): flag(id, "predict_coarse")

    # out
    pre = os.path.join(dir, id)
    txt = ("  - %s %s [[file:%s][json]] [[file:%s][log]] [[file:%s][html]] [[file:%s][png]] [[file:%s][predict log]] %s/%s %s : %s" %
           (id, lang, pre + ".json", pre + ".log", pre + ".html", pre + ".png", pre + ".predict.log",
            guess, expected, ', '.join(items.get(id, [])), comment.get(id, "")))
    if id in comment: del comment[id]

    out.write(txt + "\n")

out.write("\n\n* stats\n")
out.write("  - count: %d\n" % count)
out.write("  - fail_count: %d (%.2f%%)\n" % (fail_count, 100. * fail_count / count))
for flag, flag_count in sorted(flags.items(), key = lambda x: x[1], reverse = True):
    out.write("  - %s: %d (%.2f%%)\n" % (flag, flag_count, 100. * flag_count / fail_count))

if comment:
    out.write("\n\n* resolved\n")
    for id, txt in comment.items():
        out.write('  - comment %s : %s\n' % (id, txt))

out.write("""
* org-mode configuration
  :PROPERTIES:
  :VISIBILITY: folded
  :END:
#+STARTUP: showall
""")


if org_file:
    os.rename(org_file + ".tmp", org_file)
