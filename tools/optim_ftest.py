#! /usr/bin/python3

import sys
import ftest_replay as fr
import re

EPS = 0.0001

def optim(records, tools):
    param_list_req = tools.cf("optim").split(",")
    score_start = score0 = fr.play_all(records, tools)
    print("All parameters:", tools.params)

    param_list = set()
    for param in param_list_req:
        for p in tools.params:
            if p == param or re.match(param, p):
                param_list.add(p)
    print("Optimize requested:", param_list_req)
    print("Optimizing parameters:", list(param_list))

    params0 = dict(tools.params)

    tools.verbose = False

    updated = True
    it = 0
    while updated:
        it += 1
        updated = False
        for p in param_list:
            value0 = tools.cf(p)
            print("Iteration %d - Parameter %s = %.3f - score = %.4f" % (it, p, value0, score0))
            max_value = None
            max_score = score0
            step = max(0.001, value0 / 250)
            for sens in [-1, 1]:
                c = zc = 0
                last_score = score0
                while True:
                    new_value = value0 + sens * step * 2 ** c
                    if new_value < 0 or new_value > max(1, value0 * 2): break
                    c += 1
                    tools.set_param(p, new_value)
                    score = fr.play_all(records, tools, verbose = False)
                    print(" > try %s = %.3f --> score %.4f %s" % (p, new_value, score, "*" if score > max_score else ""))
                    if score <= score0:
                        zc += 2 if score < last_score else 1
                        if zc >= 10: break
                    else:
                        zc = 0

                    if score > max_score + EPS:
                        max_value = new_value
                        max_score = score

                    last_score = score

            if max_value:
                print("Param %s: value %.3f -> %.3f ... score %.4f -> %.4f" % (p, value0, max_value, score0, max_score))
                tools.set_param(p, max_value)
                score0 = max_score
                updated = True
            else:
                print("Param %s: no change" % p)
                tools.set_param(p, value0)

            log = [ "* %s: %.3f -> %.3f%s" % (q, params0[q], tools.params[q],
                                               " *" if params0[q] != tools.params[q] else "")
                    for q in sorted(param_list) ]
            log.append("* max_score: %.5f -> %.5f" % (score_start, max_score))
            print()
            print("\n".join(log))
            print()
            if "outfile" in tools.params:
                with open(tools.params["outfile"], "w") as f:
                    f.write("\n".join(log) + "\n")


tools = fr.Tools()
records = fr.cli_params(sys.argv[1:], tools)
optim(records, tools)
print(tools.params)
