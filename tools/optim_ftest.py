#! /usr/bin/python3

import sys
import ftest_replay as fr

EPS = 0.0001

def optim(records, tools):
    param_list = tools.cf("optim").split(",")
    print("Optimizing parameters:", param_list)
    score0 = fr.play_all(records, tools)
    print("All parameters:", tools.params)

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
            step = max(0.001, value0 / 1000)
            for sens in [-1, 1]:
                c = zc = 0
                while True:
                    new_value = value0 + sens * step * 2 ** c
                    if new_value < 0 or new_value > max(1, value0 * 2): break
                    c += 1
                    tools.params[p] = new_value
                    score = fr.play_all(records, tools, verbose = False)
                    print(" > try %s = %.3f --> score %.4f %s" % (p, new_value, score, "*" if score > max_score else ""))
                    if score <= score0:
                        zc += 1
                        if zc >= 7: break
                    else:
                        zc = 0

                    if score > max_score + EPS:
                        max_value = new_value
                        max_score = score

            if max_value:
                print("Param %s: value %.3f -> %.3f ... score %.4f -> %.4f" % (p, value0, max_value, score0, max_score))
                tools.params[p] = max_value
                score0 = max_score
                updated = True
            else:
                print("Param %s: no change" % p)
                tools.params[p] = value0

            print()
            for q in sorted(param_list):
                print("* %s: %.3f -> %.3f" % (q, params0[q], tools.params[q]))
            print()
            if "outfile" in tools.params:
                with open(tools.params["outfile"], "w") as f:
                    f.write("\n".join([ "%s: %.3f -> %.3f" % (p, params0[p], tools.params[p])
                                        for p in sorted(param_list) ]) + "\n")


tools = fr.Tools()
records = fr.cli_params(sys.argv[1:], tools)
optim(records, tools)
print(tools.params)
