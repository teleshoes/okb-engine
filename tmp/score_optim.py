#! /usr/bin/python3
# -*- coding: utf-8 -*-

# optimizer for new curve matching score (April 2015 and later)

import os, sys, glob
import scipy.optimize as opt
import numpy as np
import score_util

tests = score_util.load_files(glob.glob(os.path.join(sys.argv[1], '*')))

# 13/6/15 (all previous results are bogus due to bad distance)
# win=57/141, good=141/141, score=-0.999 x=[ 3.96501586  0.77561611  0.01228865  0.67839578  0.22884677  1.02839883]

init_values = [ 1,  1,  2, 0.5,  0.1,  0.2 ]

def f(args, tests):
    (final_coef_turn, final_turn_exp, final_distance_exp, final_coef_misc, final_score_v1_threshold, final_score_v1_coef) = args
    # cos/curve score: final_coef_cos, final_cos_exp, final_coef_curve, final_curve_exp
    # misc optim: (final_coef_turn, final_turn_exp, final_distance_exp, lazy_loop_bias, rt_score_coef, rt_score_coef_tip,
    # st5_score, tip_small_segment, turn_distance_score, ut_score) = args

    # final_turn_exp = 1 (override parameter value)
    # if final_coef_misc < 0: return -final_coef_misc
    # final_coef_misc = 0

    # final_distance_exp = 1
    # final_turn_exp = 2

    total = count = good = win = 0
    min_x = None
    for key, test in tests.items():
        if "test_fail" in key: continue  # don't optimize for bad test cases

        expected = test["ch"].get(test["expected"], None)
        if not expected: continue

        candidates = test["ch"].values()
        min_dist = min([c["distance"] for c in candidates])
        max_score_v1 = max([c["score_v1"] for c in candidates])
        for c in candidates:
            c["_score"] = ((final_coef_misc * c["avg_score"]["score_misc"] +
                            (- final_score_v1_coef) * max(0, max_score_v1 - final_score_v1_threshold - c["score_v1"]) +
                            # lazy_loop_bias * c["misc"].get("lazy_loop_bias", 0) +
                            # rt_score_coef * c["misc"].get("rt_score_coef", 0) +
                            # rt_score_coef_tip * c["misc"].get("rt_score_coef_tip", 0) +
                            # st5_score * c["misc"].get("st5_score", 0) +
                            # tip_small_segment * c["misc"].get("tip_small_segment", 0) +
                            # turn_distance_score * c["misc"].get("turn_distance_score", 0) +
                            # ut_score * c["misc"].get("ut_score", 0) +
                            final_coef_turn * (c["avg_score"]["score_turn"] ** final_turn_exp) +
                            # final_coef_cos * (max(0, c["avg_score"]["score_cos"]) ** final_cos_exp) +
                            # final_coef_curve * (max(0, c["avg_score"]["score_curve"]) ** final_curve_exp) +
                            1 - 0.1 * (float(c["distance"] - min_dist) / 10) ** final_distance_exp) /
                            (final_coef_turn + 1))


        exp_score = expected["_score"]
        max_score = max([ c["_score"] for c in candidates if c["name"] != expected["name"] ] or [ exp_score ])
        x = (exp_score - max_score) * 10

        if not min_x or x < min_x: min_x = x

        # value = x - x ** 2 if x < 0 else x / (1 + x)
        # value = math.tanh(x * 20)
        # value = math.tanh((x + 1) * 20)
        # value = 1 - x ** 4 if x < 0 else 1
        # value = 1 + x if x < 0 else 1
        value = x - x ** 4 if x < 0 else 0
        value = - value

        win += 1 if x > 0 else 0
        good += 1 if x > -1 else 0
        count += 1
        total += value

    print("win=%d/%d, good=%d/%d, score=%.3f x=%s" % (win, count, good, count, total / count, args))

    return total / count
    # return - min_x

# we have only floating point parameters, so let's just use scipy for optimization

# misc_optim: x0 = np.array([ 1.0, 0.5, 1, 0.005, 0.5, 0.1, 0.01, 0.01, 1.0, 0.5 ])
# x0 = np.array([ 1.0, 0.5, 0.1, 1, 0.1, 0.5, 0.1, 0.5 ])
# x0 = np.array([ 1.0, 0.5, 0.1, 1, 0.1, 0.02 ])
# x0 = np.array([ 1.0, 1, 0.1, 1, 0.1, 0.02 ])
x0 = np.array(init_values)

print(f(x0, tests))

result = opt.fmin_bfgs(f, x0, args = (tests,), full_output = True)
print(result)
print("== Result ==")
f(result[0], tests)
