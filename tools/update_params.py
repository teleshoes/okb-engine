#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys
import optim
import os
import re

os.chdir(os.path.dirname(sys.argv[0]))

todo = [
    ( 'DECL',     '  __type__ __param__;' ),
    ( 'DEFAULT',  '  __value__, // __param__' ),
    ( 'FROMJSON', '  p.__param__ = json["__param__"].toDouble();' ),
    ( 'TOJSON',   '  json["__param__"] = __param__;' ),
]

replace = dict()
params = optim.params
for step in todo:
    (tag, template) = step
    new_text = ""
    for p in sorted(params):
        line = template
        line = re.sub(r'__param__', p, line)
        line = re.sub(r'__type__', params[p]["type"].__name__, line)
        line = re.sub(r'__value__', str(params[p]["value"]), line)
        new_text += line + "\n"
        replace[tag] = new_text


repl = False
for line in sys.stdin.readlines():
    line = line.rstrip()
    if repl:
        mo = re.search(r'/\*\ END\ ([A-Z]+)\ \*/', line)
        if mo:
            print(line)
            repl = False
    else:
        print(line)
        mo = re.search(r'/\*\ BEGIN\ ([A-Z]+)\ \*/', line)
        if mo:
            repl = True
            print(replace[mo.group(1)])
