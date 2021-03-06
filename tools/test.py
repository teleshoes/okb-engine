#! /usr/bin/python3
# -*- coding: utf-8 -*-

import optim
import os, sys
import pickle
import getopt

ERR = 0.005

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
            if value < -10:
                if self.color_ok: return "\x1b[1;35m%s\x1b[0m" % text1
                else: return "[X]" + text1
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
    print("Usage: ", os.path.basename(__file__), " <options> [<compare file>]")
    print("Options :")
    print("-d <dump dir> : dump all input / output / result")
    print("-n : do not update compare file")
    print("-g : disable debug & logging (faster)")
    print("-T <dirs> : comma separated test dir (comma separated)")
    print("-p <params> : override parameters value (key1=value1,key2=value2)")
    print("-t <types> : comma separated scores to use")
    exit(1)

if __name__ == "__main__":
    dump_dir = None
    try:
        opts, args =  getopt.getopt(sys.argv[1:], 'd:nt:gT:p:h')
    except:
        usage()

    listpara = None
    save = True
    nodebug = False
    typs = ["max", "max2", "guess" ]
    test_dir = None
    override_params = None

    for o, a in opts:
        if o == "-d":
            dump_dir = os.path.realpath(a)
        elif o == "-n":
            save = False
        elif o == "-t":
            typs = a.split(',')
        elif o == "-g":
            nodebug = True
        elif o == "-T":
            test_dir = [ os.path.realpath(x) for x in a.split(',') ]
        elif o == "-p":
            override_params = a
        elif o == "-h":
            usage()
        else:
            print("Bad option: %s" % o)
            usage()

    os.chdir(os.path.dirname(sys.argv[0]))

    if dump_dir and not os.path.isdir(dump_dir): os.mkdir(dump_dir)

    params = optim.params

    if override_params:
        for p in override_params.split(','):
            key, value = p.split('=')
            params[key]["value"] = params[key]["type"](value)

    tests = optim.load_tests(test_dir)

    color = False
    if sys.stdout.isatty():
        import curses
        curses.setupterm()
        color = curses.tigetnum("colors") > 2

    c = Color(fname = args[0] if len(args) >= 1 else None, color_ok = color)

    for typ in typs:
        detail = dict()
        score, cputime, ok_pct = optim.run_all(tests, params, typ, fail_on_bad_score = False, return_dict = detail,
                                               silent = True, dump = dump_dir, nodebug = nodebug)

        print("### score", c.var(typ, score, "score.%s" % typ, noreg = True),
              ' '.join([ c.var(w, s, "word.%s.%s" % (typ, w))
                         for (w, s) in sorted(detail.items()) ]))
        print("cputime %.2f - found %.2f%%" % (cputime, 100. * ok_pct))

    if save: c.save()

    if c.reg: print("Regression detected:", ' '.join(c.reg))
    # exit(1 if c.reg else 0)
