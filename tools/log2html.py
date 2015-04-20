#! /usr/bin/python3
# -*- coding: utf-8 -*-


import json
from html import HTML
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

parsedate = lambda d: calendar.timegm(dateutil.parser.parse(d).timetuple())

current = None
history = []

for line in sys.stdin.readlines():
    line = line.strip()

    if line.startswith("OKBoard log"): continue

    if line.startswith("==WORD=="):
        (dummy, id, word, device_id, date) = line.split(' ', 4)
        ts = int(id.split('-')[0])
        word = re.sub(r'\'', '', word)
        current = dict(id = id, ts = ts, word = word, device_id = device_id, date = date, content = "", js = None)
        history.append(current)
        continue

    if not current: continue

    if line.startswith('OUT:'):
        js = line[4:].strip()
        current["js"] = json.loads(js)
        continue

    current["content"] += line + "\n"

history.sort(key = lambda x: x["ts"], reverse = True)

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

for item in history:
    word = item["word"]
    ts = item["ts"]
    js = item["js"]
    id = item["id"]
    content = item["content"]
    device_id = item["device_id"]

    body.h5("Word: %s [%s] %s %s" % (word, time.ctime(ts), device_id, id))

    if js:
        li = body.p("Details: ")
        li.a("json", href = "%s.json" % id)
        li += " - "
        name = "%s-%s.html" % (word, id)
        li.a("curve show", href = name)
        trefile = os.path.join(mydir, "../db", os.path.basename(js["input"]["treefile"]))
        htmlfile = os.path.join(out, name)

        if not os.path.isfile(htmlfile):
            jsonfile = os.path.join(out, "%s.json" % id)
            with open(jsonfile, "w") as f:
                f.write(json.dumps(js))

            print("%s -> %s" % (word, htmlfile))
            pool.apply_async(json2html, args = (htmlfile, trefile, json.dumps(js)))

    body.p
    body.pre(content)
    body.hr

pool.close()
pool.join()

with open(os.path.join(out, "index.html"), "w") as f:
    f.write(str(html))
