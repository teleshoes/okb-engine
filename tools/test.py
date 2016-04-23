#! /usr/bin/python3
# -*- coding: utf-8 -*-

import optim
import os, sys
import pickle
import getopt

ERR = 0.05

def err(x):
    return max(ERR, abs(x) * ERR)

class Color:
    def __init__(self, fname = None, color_ok = False):
        self.db = dict()
        self.fname = fname
        self.reg = []
        self.color_ok = color_ok
        if fname:
            try: self.db = pickle.load(open(fname, 'rb'))
            except: pass

    def var(self, label, value, key, noreg = False):
        text1 = "%s[%.2f]" % (label, value)
        if key not in self.db:
            self.db[key] = value
            return text1

        if not self.fname: return text1

        old_value = self.db[key]
        self.db[key] = value
        text2 = "%s[%.2f<<%.2f]" % (label, value, old_value)

        if value > old_value + err(old_value):
            if self.color_ok: return "\x1b[1;32m%s\x1b[0m" % text2
            else: return "[+]" + text2

        if value < old_value - err(old_value):
            if noreg: self.reg.append("%s: %.2f -> %.2f" % (key, old_value, value))
            if self.color_ok: return "\x1b[1;31m%s\x1b[0m" % text2
            else: return "[-]" + text2

        return text1

    def save(self):
        if self.fname: pickle.dump(self.db, open(self.fname, 'wb'))

def usage():
    print("Usage: ", os.path.basename(__file__), " [-d <dump dir>] [-n] [<compare file>]")
    exit(1)

if __name__ == "__main__":
    os.chdir(os.path.dirname(sys.argv[0]))

    dump_dir = None
    try:
        opts, args =  getopt.getopt(sys.argv[1:], 'd:n')
    except:
        usage()

    listpara = None
    save = True
    for o, a in opts:
        if o == "-d":
            dump_dir = a
        elif o == "-n":
            save = False
        else:
            print("Bad option: %s" % o)
            usage()

    if dump_dir and not os.path.isdir(dump_dir): os.mkdir(dump_dir)

    params = optim.params
    tests = optim.load_tests()

    color = False
    if sys.stdout.isatty():
        import curses
        curses.setupterm()
        color = curses.tigetnum("colors") > 2

    c = Color(fname = args[0] if len(args) >= 1 else None, color_ok = color)

    for typ in ["max", "max2", "good", "good2", "guess" ]:
        detail = dict()
        score = optim.run_all(tests, params, typ, fail_on_bad_score = False, return_dict = detail, silent = True, dump = dump_dir)

        print("### score", c.var(typ, score, "score.%s" % typ, noreg = True),
              ' '.join([ c.var(w, s, "word.%s.%s" % (typ, w))
                         for (w, s) in sorted(detail.items()) ]))

    if save: c.save()

    if c.reg: print("Regression detected:", ' '.join(c.reg))
    # exit(1 if c.reg else 0)
