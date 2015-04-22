#! /usr/bin/python3
# -*- coding: utf-8 -*-

# HTML dashboard for new curve matching score (April 2015)

import sys, os
import math
import score_util

mydir = os.path.dirname(__file__)
libdir = os.path.join(mydir, '..', 'tools')
sys.path.insert(0, libdir)

from html import HTML  # https://pypi.python.org/pypi/html


# --- actions
def check_min(test, get_attr, name, colors = None):
    expected = test["ch"].get(test["expected"], None)
    if not expected: return dict(txt="Word not found", color="red", count=[ "not_found" ])

    exp_val = get_attr(expected)
    min_val = min([ get_attr(x) for x in test["ch"].values() if x["name"] != test["expected"] ] or [ exp_val ])
    gap = min_val - exp_val
    ratio = min_val / exp_val

    if exp_val <= min_val:
        return dict(txt="OK %s[%s]=%.2f gap=%.2f ratio=%.2f" % (name, test["expected"], exp_val, gap, ratio), count = [ "ok" ])

    rank = [ get_attr(x) for x in test["ch"].values() ].index(exp_val)
    return dict(txt="NOK %s[%s]=%.2f min=%.2f gap=%.2f ratio=%.2f rank=%d" % (name, test["expected"], exp_val, min_val, gap, ratio, rank), color="yellow")

def check_max(test, get_attr, name, colors):
    expected = test["ch"].get(test["expected"], None)
    if not expected: return dict(txt="Word not found", color="red", count=[ "not_found" ])

    exp_val = get_attr(expected)
    max_val = max([ get_attr(x) for x in test["ch"].values() if x["name"] != test["expected"] ] or [ exp_val ])
    gap = exp_val - max_val

    color = "purple"
    for c in colors:
        if gap > c[0]:
            color = c[1]
            break

    return dict(txt="%s[%s]=%.3f gap=%.3f" % (name, test["expected"], max_val, gap), color = color, count = [ color ])

COLORS = [ (0.01, "green"), (-0.01, ""), (-0.05, "yellow"), (-0.1, "orange"), (-0.15, "red") ]

def act_distance(test):
    return check_min(test, lambda c: c["distance"], "distance")

def act_distance_adj(test):
    return check_min(test, lambda c: c["distance"] / math.sqrt(len(c["name"])), "distance_adj")

def act_score_misc(test):
    return check_max(test, lambda c: c["avg_score"]["score_misc"], "score_misc", colors = COLORS)

def act_score_turn(test):
    return check_max(test, lambda c: c["avg_score"]["score_turn"], "score_turn", colors = COLORS)

def act_score_turn_min(test):
    return check_max(test, lambda c: c["min_score"]["score_turn"], "score_turn", colors = COLORS)

def act_score_curve(test):
    return check_max(test, lambda c: c["avg_score"]["score_curve"], "score_curve", colors = COLORS)

def act_score_cos(test):
    return check_max(test, lambda c: c["avg_score"]["score_cos"], "score_cos", colors = COLORS)

def act_final_score_calc(test):
    candidates = test["ch"].values()
    min_dist = min([x["distance_adj"] for x in candidates])
    for c in candidates:
        c["final_score"] = (1.0 - (c["distance_adj"] - min_dist) * 0.01
                            + 0.9 * (c["avg_score"]["score_turn"] ** 0.1)
                            + 0.1 * c["avg_score"]["score_misc"]) / 1.9

    return check_max(test, lambda c: c["final_score"], "final_score", colors = COLORS)

def act_final_score(test):
    return check_max(test, lambda c: c["score"], "final_score", colors = COLORS)

def act_threshold(test):
    expected = test["ch"].get(test["expected"], None)
    if not expected: return dict(txt="Word not found", color="red", count=[ "not_found" ])
    candidates = test["ch"].values()

    return dict(txt="%.3f:%.3f" % (
        expected["score"] - max([ c["score"] for c in candidates ]),
        expected["score_v1"] - max([ c["score_v1"] for c in candidates ])
    ))

all_actions = dict(distance=act_distance,
                   distance_adj=act_distance_adj,
                   score_turn=act_score_turn,
                   score_misc=act_score_misc,
                   threshold=act_threshold,
                   # score_cos=act_score_cos,
                   # score_curve=act_score_curve,
                   # score_turn_min=act_score_turn_min,
                   # _final_score_calc=act_final_score_calc,
                   _final_score=act_final_score
                   )

# --- read files
tests = score_util.load_files(os.listdir("."))

# --- run actions
result = dict()
counters = dict()
for t, js in tests.items():
    result[t] = dict()
    for action, func in all_actions.items():
        result[t][action] = out = func(js)
        if "count" in out:
            for k in out["count"]:
                if action not in counters: counters[action] = dict()
                if k not in counters[action]: counters[action][k] = 0
                counters[action][k] += 1

# --- HTML
title = 'OKBoard scores'

html = HTML()
hd = html.head
hd.title(title)
hd.meta(charset = "utf-8")
body = html.body
body.h3(title)

tbl = body.table(border = "1")
hr = tbl.tr
columns = sorted(all_actions.keys())
for c in [ "test" ] + columns:
    hr.td(c, align = "center", bgcolor="#C0FFC0")

tr = tbl.tr
tr.td("Total", bgcolor="#c0c0c0")
for action in columns:
    total = []
    if action in counters:
        for c, v in counters[action].items():
            total.append("count-%s: %d (%.1f%%)" % (c or "none", v, 100.0 * v / len(tests)))
    tr.td(" ".join(sorted(total)), bgcolor="#c0c0c0")

for t in sorted(tests.keys()):
    tr = tbl.tr
    tr.td.a(t, href=tests[t]["html"])

    for action in columns:
        rs = result[t][action]
        tr.td(rs.get("txt", "-"), bgcolor=rs.get("color", ""))

print(html)
