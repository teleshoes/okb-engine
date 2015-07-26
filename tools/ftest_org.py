#! /usr/bin/python3
# -*- coding: utf-8 -*-


import sys, re, os
import json, time
import getopt

mydir = os.path.dirname(os.path.abspath(__file__))
mydir = libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import predict

add_new = False
opts, args =  getopt.getopt(sys.argv[1:], 'a')
for o, a in opts:
    if o == "-a": add_new = True
    else:
        print("Bad option: %s" % o)
        exit(1)

if len(args) < 2:
    print("usage:", os.path.basename(__file__), "[-a] <in/out org index file> <work directory> [<option key=value> ... ]")
    print(" (it reads logs from stdin)")
    exit(1)

index_org, work_dir = args[0:2]
params = dict( [ x.split('=', 1) for x in args[2:] ] )

current = None
history = []
last_ts = 0

def remove_quotes(word):
    return  re.sub(r'^([\'\"])(.*)\1$', r'\2', word)

for line in sys.stdin.readlines():
    line = line.strip()

    if line.startswith("==WORD=="):
        (dummy, id, word, device_id, date) = line.split(' ', 4)
        last_ts = ts = int(id.split('-')[0])
        word = remove_quotes(word)
        current = dict(id = id, ts = ts, word = word, device_id = device_id, date = date, content = "", js = None)
        continue

    if not current:
        mo = re.search(r'Learn:\s*(w+)\s*\[([^\[\]]*)\].*replaces\s+(\w+)', line)
        if mo:
            word, context, replaces = mo.group(1), mo.group(2), mo.group(3)
            context = [ remove_quotes(x.strip()) for x in context.split(',') ]
            for i in range(- min(20, len(history)), 0):
                l = min(len(history[i].context), len(context))
                if history[i].context[:l] == context[:l] and history[i].word == replaces:
                    history[i].replaces = replaces
                    history[i].word = word
                    break

        continue

    if line.startswith('OUT:'):
        js = line[4:].strip()
        current["js"] = js
        continue

    mo = re.search(r'^ID:.*Context:\s*\[([^\[\]]*)\]', line)
    if mo:
        context = current["context"] = [ remove_quotes(x.strip()) for x in mo.group(1).split(',') ]
        continue

    mo = re.search(r'^Selected word:\s*(\w+)', line)
    if mo:
        current["word"] = current["guessed"] = mo.group(1)
        if "js" in current: history.append(current)
        current = None

history.sort(key = lambda x: x["id"], reverse = False)

class Tools:
    def __init__(self, params = dict()):
        self.messages = []
        self.params = params

    def log(self, *args, **kwargs):
        if not args: return
        message = ' '.join(map(str, args))
        print(message)
        self.messages.append(message)

    def cf(self, key, default_value, cast):
        return cast(self.params.get(key, default_value))

tools = Tools(params)

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
            context = list(reversed(re.split(r'\s+', li)))
            word = context.pop(0)
            db[id] = (check == 'X'), word, context

# write org file
f = open(index_org + ".tmp", 'w')
f.write("# OKBoard replay\n\n[%s] Parameters: %s\n\n" % (time.ctime(), params))
last_date = None
count = ok_count = 0
last_lang = None
p = None

for t in history:
    id = t["id"]
    ts = t["ts"]
    word = t["word"]
    context = t["context"]
    pre = os.path.join(work_dir, id)
    result = json.load(open(pre + ".json", 'r'))

    lang = os.path.basename(result["input"]["treefile"])[:2]

    check = True
    comment = False
    if id in db:
        check, word, context = db[id]
    elif not add_new:
        comment = True

    if lang != last_lang:
        p = predict.Predict(tools)
        db_file = os.path.join(mydir, "db/predict-%s.db" % lang)
        p.set_dbfile(db_file)
        p.load_db()
        last_lang = lang

    prefix = "  - [%s] %s %s [[file:%s][json]] [[file:%s][html]] [[file:%s][png]] [[file:%s][predict log]] %s %s" % \
             ("X" if check else " ", id, lang, pre + ".json", pre + ".html", pre + ".png", pre + ".predict.log",
              " ".join(reversed(context)), word)

    if comment: prefix = "  # use -a option to add: " + prefix.strip()

    date = time.strftime("%Y-%m-%d %a", time.localtime(ts))
    if last_date != date:
        last_date = date
        f.write("* <%s>\n" % date)

    if not check or comment:
        f.write("%s\n" % prefix)
        continue

    tmp = " ".join(reversed(context))
    p.update_surrounding(tmp, len(tmp))
    p.update_preedit("")
    p._update_last_words()
    print("====", id, " ".join(reversed(context)), word)

    candidates = [ (c["name"], c["score"], c["class"], c["star"], c["words"]) for c in result["candidates"] ]
    guess = p.guess(candidates, speed = result["stats"]["speed"], show_all = True)

    ok = (guess == word)
    count += 1
    if ok: ok_count += 1

    if not ok:
        candidates.sort(key = lambda x: x[1], reverse = True)
        rank = -1
        for i in range(len(candidates)):
            x = candidates[i]
            if word == x[0] or re.match(r'\b' + word + r'\b', x[4]):
                rank = i
                break

    # @todo add wiring to learning / backtracking

    with open(pre + ".predict.log", 'w') as pf:
        pf.write("\n".join(tools.messages))
    tools.messages = []

    f.write("%s -> %s\n" % (prefix, "_OK_" if ok else ("*FAIL* (%s, rank=#%d)" % (guess, rank))))

stats = "Total=%d OK=%d rate=%.2f%%" % (count, ok_count, 100.0 * ok_count / count)
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
os.rename(index_org + ".tmp", index_org)
