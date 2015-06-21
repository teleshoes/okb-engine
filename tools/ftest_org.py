#! /usr/bin/python3
# -*- coding: utf-8 -*-


import sys, re, os
import json, time

mydir = os.path.dirname(os.path.abspath(__file__))
mydir = libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import predict

if len(sys.argv) != 3:
    print("usage:", os.path.basename(__file__), "<in/out org index file> <work directory>")
    print(" (it reads logs from stdin)")
    exit(1)

index_org, work_dir = sys.argv[1:]

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

history.sort(key = lambda x: x["ts"], reverse = False)

class Tools:
    def __init__(self):
        self.messages = []

    def log(self, *args, **kwargs):
        if not args: return
        message = ' '.join(map(str, args))
        print(message)
        self.messages.append(message)

    def cf(self, key, default_value, cast):
        return cast(default_value)

tools = Tools()

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
f.write("# OKBoard replay\n\n")
predict_lang = dict()
last_date = None
count = ok_count = 0
for t in history:
    id = t["id"]
    ts = t["ts"]
    word = t["word"]
    context = t["context"]
    pre = os.path.join(work_dir, id)
    result = json.load(open(pre + ".json", 'r'))

    lang = os.path.basename(result["input"]["treefile"])[:2]

    check = True
    if id in db:
        check, word, context = db[id]

    if lang not in predict_lang:
        p = predict.Predict(tools)
        db_file = os.path.join(mydir, "db/predict-%s.db" % lang)
        p.set_dbfile(db_file)
        p.load_db()
        predict_lang[lang] = p
    else:
        p = predict_lang[lang]

    prefix = "  - [%s] %s [[file:%s][json]] [[file:%s][html]] [[file:%s][png]] [[file:%s][predict log]] %s %s" % \
             ("X" if check else " ", id, pre + ".json", pre + ".html", pre + ".png", pre + ".predict.log",
              " ".join(reversed(context)), word)

    date = time.strftime("%Y-%m-%d %a", time.localtime(ts))
    if last_date != date:
        last_date = date
        f.write("* <%s>\n" % date)

    if not check:
        f.write("%s\n" % prefix)
        continue


    tmp = " ".join(reversed(context))
    p.update_surrounding(tmp, len(tmp))
    p.update_preedit("")
    p._update_last_words()
    print("====", id, " ".join(reversed(context)), word)

    candidates = [ (c["name"], c["score"], c["class"], c["star"], c["words"]) for c in result["candidates"] ]
    guess = p.guess(candidates, speed = result["stats"]["speed"])

    ok = (guess == word)
    count += 1
    if ok: ok_count += 1

    if not ok:
        candidates.sort(key = lambda x: x[1], reverse = True)
        try: rank = candidates.index(word)
        except: rank = -1

    # @todo add wiring to learning / backtracking

    with open(pre + ".predict.log", 'w') as pf:
        pf.write("\n".join(tools.messages))
    tools.messages = []

    f.write("%s -> %s\n" % (prefix, "_OK_" if ok else ("*FAIL* (%s, rank=#%d)" % (guess, rank))))

stats = "Total=%d OK=%d rate=%d%%" % (count, ok_count, int(100.0 * ok_count / count))
print(stats)

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
