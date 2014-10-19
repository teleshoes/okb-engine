#! /usr/bin/python3
# -*- coding: utf-8 -*-

import json
from html import HTML  # https://pypi.python.org/pypi/html (it's great!)
import sys, re, os
from multiprocessing.pool import ThreadPool
import multiprocessing
import dateutil.parser
import calendar
import time
import subprocess

if len(sys.argv) < 2:
    print("usage:", os.basename(__file__), "<output directory>")
    exit(1)

out = sys.argv[1]
mydir = os.path.dirname(__file__)

curves = []
predicts = []

pts, ptxt, pid, pword = None, None, None, None

parsedate = lambda d: calendar.timegm(dateutil.parser.parse(d).timetuple())

for line in sys.stdin.readlines():
    line = line.strip()
    if line.startswith("IN:"): continue
    if line.startswith("OUT:"):
        js = json.loads(line[4:].strip())
        ts = parsedate(js["ts"])
        id = int(js["id"])
        curves.append((ts, id, js))
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

# pool
cpus = multiprocessing.cpu_count()
pool = ThreadPool(processes = cpus)

# generate HTML
title = 'OKboard logs'

html = HTML()
hd = html.head
hd.title(title)
hd.meta(charset = "utf-8")
body = html.body
body.h3(title)

def json2html(fname, tre, js):
    try:
        sp = subprocess.Popen([ os.path.join(mydir, "jsonresult2html.py"), tre ],
                              stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        (out, err) = sp.communicate(bytes(js, "UTF-8"))
        with open(fname, "w") as f:
            f.write(out.decode(encoding = 'UTF-8'))
    except Exception as e:
        print("BOUM", e)

for ts, id, txt, word in reversed(predicts):
    body.h5("Word: %s [%s]" % (word, time.ctime(ts)))

    cl = [ x for x in curves if x[1] == id and abs(x[0] - ts) < 30 ]
    if cl:
        js = cl[0][2]
        sid = "%d-%d" % (ts, id)
        with open(os.path.join(out, "%s.json" % sid), "w") as f:
            f.write(json.dumps(js["input"]))
        with open(os.path.join(out, "%s-out.json" % sid), "w") as f:
            f.write(json.dumps(js))
        li = body.p("Details: ")
        li.a("json", href = "%s.json" % sid)
        li += " - "
        li.a("result json", href = "%s-out.json" % sid)
        li += " - "
        name = "%s-%d-%d.html" % (word, ts, id)
        li.a("curve show", href = name)
        trefile = os.path.join(mydir, "../db", os.path.basename(js["input"]["treefile"]))
        htmlfile = os.path.join(out, name)
        if not os.path.isfile(htmlfile):
            print("%s -> %s" % (word, htmlfile))
            pool.apply_async(json2html, args = (htmlfile, trefile, json.dumps(js)))
            # json2html(htmlfile, trefile, json.dumps(js))
    body.p
    body.pre('\n'.join(txt))
    body.hr

pool.close()
pool.join()

with open(os.path.join(out, "index.html"), "w") as f:
    f.write(str(html))
