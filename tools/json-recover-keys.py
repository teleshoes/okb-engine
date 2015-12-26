#! /usr/bin/python
# ugly workaround for some old json files with lost key section

import json, sys

persist_file = sys.argv[1]

js = json.load(sys.stdin)

ptr = js
if "input" in ptr: ptr = ptr["input"]

if len(ptr["keys"]):
    json.dump(ptr["keys"], open(persist_file, 'wb'))
else:
    ptr["keys"] = json.load(open(persist_file, 'rb'))
    sys.stderr.write("keys section recovered ...\n")

json.dump(js, sys.stdout)
