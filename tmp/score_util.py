#! /usr/bin/python3
# -*- coding: utf-8 -*-

import re, os, json, math
# import sys

def load_files(files):
    tests = dict()
    for jsfile in [ f for f in files if f.endswith('.json') and f.find('#') == -1 ]:
        js = json.loads(open(jsfile, 'r').read())

        word = os.path.basename(jsfile)
        word = word[:-5]
        word = re.sub(r'^.*\.', '', word)
        word = re.sub(r'^[a-z][a-z]-', '', word)
        word = re.sub(r'[0-9]+$', '', word)
        word = re.sub(r'(.)\1+', lambda m: m.group(1), word)

        js['expected'] = word
        js['html'] = jsfile[:-5] + ".html"

        ch = js['ch'] = dict()
        for c in js['candidates']:
            # new attributes
            c["distance_adj"] = c["distance"] / math.sqrt(len(c["name"]))

            ch[c['name']] = c
            c["misc"] = dict()

        with open(jsfile[:-5] + ".log") as f:  # read "misc" score value from log file
            for line in f.readlines():
                line = line.strip()
                if not re.search(r'\[\*\]\ MISC', line): continue
                (dum1, dum2, name, coef_name, coef_value, value) = line.split(' ')

                if name not in ch: continue
                if coef_name not in ch[name]["misc"]: ch[name]["misc"][coef_name] = 0.0
                ch[name]["misc"][coef_name] += float(value)

        tests[jsfile] = js

    # sys.stderr.write("%s\n" % tests)

    return tests
