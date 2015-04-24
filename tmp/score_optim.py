#! /usr/bin/python3
# -*- coding: utf-8 -*-

# optimizer for new curve matching score (April 2015)

import os, sys, glob
import scipy.optimize as opt
import numpy as np
import score_util
import math

tests = score_util.load_files(glob.glob(os.path.join(sys.argv[1], '*')))

# final score = (score_misc + final_coef_turn * score_turn ^ (final_turn_exp) + f(distance)) * (normalization coef)
# http://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.fmin_bfgs.html

# misc scores:
#      18518 lazy_loop_bias 0.005000
#       6519 rt_score_coef 0.500000
#       5589 rt_score_coef_tip 0.100000
#       1554 st5_score 0.010000
#       3840 tip_small_segment 0.010000
#       3209 turn_distance_score 1.000000
#        474 ut_score 0.500000

# run2 : [ 0.87696685,  0.09290564,  0.50442763,  0.03203242,  0.11457762,
#          -0.05446582,  0.01953923,  0.01934534, -0.2606125 ,  0.05651059]

# run 24/04/15 :
#  [  0.11329048,  13.59298864,   0.77919986,   1.46618334,   0.05407549,   4.08000888,   3.67530324,   0.04451949] <- crap local minimum
#  [ 0.98203151,  0.46720921,  0.17562272,  0.97977564,       0.14050587, 0.50725596,  0.07673429,  0.49549159] <- good!

def f(args, tests):
    (final_coef_turn, final_turn_exp, final_distance_exp, final_coef_misc,
     final_coef_cos, final_cos_exp,
     final_coef_curve, final_curve_exp) = args

    # misc optim: (final_coef_turn, final_turn_exp, final_distance_exp, lazy_loop_bias, rt_score_coef, rt_score_coef_tip,
    # st5_score, tip_small_segment, turn_distance_score, ut_score) = args

    total = count = good = win = 0
    for test in tests.values():
        expected = test["ch"].get(test["expected"], None)
        if not expected: continue

        candidates = test["ch"].values()
        min_dist = min([c["distance_adj"] for c in candidates])
        for c in candidates:
            c["_score"] = ((final_coef_misc * c["avg_score"]["score_misc"] +
                            # lazy_loop_bias * c["misc"].get("lazy_loop_bias", 0) +
                            # rt_score_coef * c["misc"].get("rt_score_coef", 0) +
                            # rt_score_coef_tip * c["misc"].get("rt_score_coef_tip", 0) +
                            # st5_score * c["misc"].get("st5_score", 0) +
                            # tip_small_segment * c["misc"].get("tip_small_segment", 0) +
                            # turn_distance_score * c["misc"].get("turn_distance_score", 0) +
                            # ut_score * c["misc"].get("ut_score", 0) +
                            final_coef_turn * (c["avg_score"]["score_turn"] ** final_turn_exp) +
                            final_coef_cos * (max(0, c["avg_score"]["score_cos"]) ** final_cos_exp) +
                            final_coef_curve * (max(0, c["avg_score"]["score_curve"]) ** final_curve_exp) +
                            1 - 0.1 * (0.1 * (c["distance_adj"] - min_dist)) ** final_distance_exp) /
                            (final_coef_turn + 1))


        exp_score = expected["_score"]
        max_score = max([ c["_score"] for c in candidates if c["name"] != expected["name"] ] or [ exp_score ])
        x = (exp_score - max_score) * 10

        #value = x - x ** 2 if x < 0 else x / (1 + x)
        value = math.tanh(x * 20)
        value = - value

        win += 1 if x > 0 else 0
        good += 1 if x > -1 else 0
        count += 1
        total += value

    print("win=%d/%d, good=%d/%d, score=%.3f x=%s" % (win, count, good, count, total / count, args))

    return total / count

# we have only floating point parameters, so let's just use scipy for optimization

# misc_optim: x0 = np.array([ 1.0, 0.5, 1, 0.005, 0.5, 0.1, 0.01, 0.01, 1.0, 0.5 ])
x0 = np.array([ 1.0, 0.5, 0.1, 1, 0.1, 0.5, 0.1, 0.5 ])

print(f(x0, tests))

result = opt.fmin_bfgs(f, x0, args = (tests,), full_output = True)
print (result)
