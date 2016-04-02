#! /usr/bin/python3
# -*- coding: utf-8 -*-

import json
import sys, re, os
import dateutil.parser
import calendar
import time
import bz2

if len(sys.argv) < 2:
    print("usage:", os.path.basename(__file__), "<output directory> [<id>]")
    exit(1)

out = sys.argv[1]
mydir = os.path.dirname(__file__)

becane_id = sys.argv[2] if len(sys.argv) >= 3 else "<NO-ID>"

curves = []
predicts = []

pts, ptxt, pid, pword = None, None, None, None

parsedate = lambda d: calendar.timegm(dateutil.parser.parse(d).timetuple())

for line in sys.stdin.readlines():
    line = line.strip()
    if line.startswith("IN:"): continue
    if line.startswith("OUT:"):
        txt = line[4:].strip()
        js = json.loads(txt)
        ts = parsedate(js["ts"])
        id = int(js["id"])
        curves.append((ts, id, js, txt))
        continue

    if re.search(r'^[A-Z][a-z][a-z] [A-Z][a-z][a-z] [0-9][0-9]', line):
        # new prediction (old style)
        if pts: predicts.append((pts, pid, ptxt, pword))
        line = re.sub(r'\s*\-.*$', '', line)
        pts = parsedate(line)
        ptxt = []
        pid = -1
        pword = None
    else:
        # new prediction (current format)
        mo = re.search(r'^\[\*\]\s+([^\-]+\S)\s+\-', line)
        if not mo: mo = re.search(r'^\[\*\]\s+([^\-]+\S)\s*$', line)  # slightly changed log format
        if mo:
            if pts: predicts.append((pts, pid, ptxt, pword))
            pts = parsedate(mo.group(1))
            ptxt = []
            pid = -1
            pword = None

    if pts:
        mo = re.search(r'^ID:\s*(\d+)', line)
        if mo: pid = int(mo.group(1))

        mo = re.search(r'^Selected word:\s*(\S+)', line)
        if mo: pword = mo.group(1)

        ptxt.append(line)

if pts: predicts.append((pts, pid, ptxt, pword))

curfile = None

def save(x):
    if not x or not x["modified"]: return
    fname = x["fname"]
    items = x["items"]
    d = os.path.dirname(fname)
    print("Saving:", fname)
    if not os.path.isdir(d): os.mkdir(d)

    with bz2.open(fname + ".tmp", 'wb') as f:
        f.write(bytes("OKBoard log processed on %s\n\n" % time.ctime(time.time()), 'UTF-8'))
        keys = list(items.keys())
        keys.sort(key = lambda x: int(x.split('-')[0]))

        for key in keys:
            item = items[key]
            txt = '\n'.join(item["txt"])
            txt = re.sub(r'^\s*\n', '', txt, re.S)
            txt = re.sub(r'\n\s*$', '', txt, re.S)
            if "title" in item:
                f.write(bytes("%s\n" % item["title"], 'UTF-8'))
            else:
                f.write(bytes("==WORD== %s '%s' %s [%s]\n\n" % (key, item["word"], item["becane_id"], time.ctime(item["ts"])), 'UTF-8'))

            if "curve" in item: f.write(bytes("OUT: %s\n\n" % item["curve"], 'UTF-8'))
            f.write(bytes("%s\n\n" % txt, 'UTF-8'))

    os.rename(fname + ".tmp", fname)

def load(fname):
    x = dict(items = dict(), modified = False, fname = fname)
    try:
        with bz2.open(fname, 'rb') as f:
            lines = f.readlines()
    except: return x
    print("Loading:", fname)

    cur = None
    for line in lines:
        line = line.decode('UTF-8').strip()
        if re.match(r'^==WORD==', line):
            cur = line.split(' ')[1]
            x["items"][cur] = dict(title = line.strip(), txt = [])
        elif cur:
            x["items"][cur]["txt"].append(line)

    return x

for ts, id, txt, word in predicts:
    cl = [ x for x in curves if x[1] == id and abs(x[0] - ts) < 30 ]
    if cl:
        curve = cl[0]
        date = time.strftime("%Y-%m-%d", time.localtime(ts))
        fname = os.path.join(out, date[0:7], "%s.log.bz2" % date)

        if not curfile or fname != curfile["fname"]:
            if curfile: save(curfile)
            curfile = load(fname)

        key = "%s-%s" % (ts, id)
        if key in curfile["items"]: continue
        curfile["items"][key] = dict(ts = ts, becane_id = becane_id, id = id, word = word, txt = txt, curve = curve[3])
        print("Adding item:", word)
        curfile["modified"] = True

save(curfile)
