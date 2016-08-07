#! /usr/bin/python3

import os, sys
import pickle
import hashlib
import json

manifest = sys.argv[1]
select_key = sys.argv[2] if len(sys.argv) >= 3 else None
out_file = sys.argv[3] if len(sys.argv) >= 4 else None

dir = os.path.dirname(manifest)

records = pickle.load(open(manifest, 'rb'))
count = dict()

err = dict()
keyboard = None

it = 0
out = []
for t in records:
    id = t["id"]
    word = t["expected"]
    cmp = t.get("compare_word", None)

    js = json.load(open(os.path.join(dir, "%s.json" % id), 'r'))
    if js.get("multi", 0): continue

    it += 1
    if not it % 100: sys.stderr.write("%d\n" % it)

    # keyboard signature
    h = hashlib.md5()
    keys = dict()
    for k in sorted(js["input"]["keys"], key = lambda x: x["k"]):
        h.update(("%s: %d,%d " % (k["k"], k["x"], k["y"])).encode('utf-8'))
        keys[k["k"]] = (k["x"], k["y"], k["h"], k["w"])
    sign = h.hexdigest()

    if sign not in count: count[sign] = 0
    count[sign] += 1

    if sign != select_key: continue

    if not keyboard: keyboard = keys

    # find candidate
    found = [ c for c in js["candidates"] if c["name"] == word or word in c["words"].split(',') ]
    if not found: continue
    candidate = found[0]

    for d in candidate["detail"]:
        l = d["letter"]
        i = d["curve_index"]
        c = js["input"]["curve"][i]

        out.append(dict(key = l,
                        kx = keys[l][0], ky = keys[l][1],
                        kw = keys[l][2], kh = keys[l][3],
                        mx = c["x"], my = c["y"], speed = c["speed"]))


if not select_key:
    for key in count:
        print("Key %s: %d" % (key, count[key]))
    exit(0)

if out_file:
    with open(out_file, 'wb') as f:
        pickle.dump(out, f)
