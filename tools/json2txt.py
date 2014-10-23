#! /usr/bin/python3
# -*- coding: utf-8 -*-

import json
import sys

txt = sys.stdin.read()
p = txt.find('{')
txt = txt[p:]

js = json.loads(txt)

n = 0
for c in sorted(js["candidates"], key = lambda x: x["score"], reverse = True):
    n += 1
    print(n, c["name"], c["score"])
