#! /usr/bin/python2
# -*- coding: utf-8 -*-

import optim
import os, sys

if __name__ == "__main__":
    os.chdir(os.path.dirname(sys.argv[0]))

    params = optim.params
    tests = optim.load_tests()

    for typ in ["max", "max2", "stddev" ]:
        detail = dict()
        score = optim.run_all(tests, params, typ, fail_on_bad_score = False, return_dict = detail, silent = True)

        print "score", typ, score, ' '.join(sorted([ "%s[%.3f]" % (w, s) for (w, s) in detail.items() ]))
