#! /usr/bin/python3
# -*- coding: utf-8 -*-

# replay functional test suite from a pickled file

import sys, os, re
import pickle

mydir = os.path.dirname(os.path.abspath(__file__))
mydir = libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

import language_model, backend

class Tools:
    def __init__(self, params = dict()):
        self.messages = None
        self.params = params
        self.verbose = False

    def log(self, *args, **kwargs):
        if not args: return
        message = ' '.join(map(str, args))
        if self.verbose: print(message)
        if self.messages is not None: self.messages.append(message)

    def cf(self, key, default_value, cast):
        if key in self.params:
            return cast(self.params[key])
        else:
            value = self.params[key] = cast(default_value)
            return value

def play_all(record, tools):
    all_langs = set([ x["lang"] for x in record ])
    count = count_ok = 0

    for lang in all_langs:
        db_file = os.path.join(mydir, "db/predict-%s.db" % lang)
        db = backend.FslmCdbBackend(db_file)
        lm = language_model.LanguageModel(db, tools = tools)

        for t in record:
            if t["lang"] != lang: continue

            details = dict()
            guesses = lm.guess(t["context"], t["candidates"],
                               correlation_id = t["id"], speed = t["speed"],
                               verbose = True, expected_test = t["expected"], details_out = details)

            ok = guesses and guesses[0] == t["expected"]
            count += 1
            if ok: count_ok += 1

        lm.close()

    print("Summary: total=%d OK=(%.2f%%)" % (count, 100.0 * count_ok / count))


if __name__ == "__main__":
    tools = Tools()
    file_in = None
    for arg in sys.argv[1:]:
        mo = re.match(r'^([0-9a-z_]+)=(.*)$', arg)
        if mo:
            tools.params[mo.group(1)] = float(mo.group(2))
        elif os.path.exists(arg):
            file_in = arg
        else:
            raise Exception("bad arg: %s" % arg)

    if not file_in: raise Exception("Add pickle file name as argument !")

    record = pickle.load(open(file_in, 'rb'))

    play_all(record, tools)
    print(tools.params)
