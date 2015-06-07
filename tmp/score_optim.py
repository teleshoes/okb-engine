#! /usr/bin/python3
# -*- coding: utf-8 -*-

# optimizer for new curve matching score (April 2015 and later)

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
# (final_coef_turn, final_turn_exp, final_distance_exp, final_coef_misc)
#  [  0.11329048,  13.59298864,   0.77919986,   1.46618334,   0.05407549,   4.08000888,   3.67530324,   0.04451949] <- crap local minimum
#  [ 0.98203151,  0.46720921,  0.17562272,  0.97977564,       0.14050587, 0.50725596,  0.07673429,  0.49549159] <- good!

# run 31/05/15 :
# (final_coef_turn, final_turn_exp, final_distance_exp, final_coef_misc, final_score_v1_threshold, final_score_v1_coef)
# [ 0.92668943,  0.35946521,  0.262141  ,  0.98711713,  0.06514241, 0.14933755]
# -> win=59/159, good=157/159, score=0.236 x=[ 0.92597021  0.35903125  0.25067485  0.98765745  0.0655745   0.14985466]
#
# [ 0.34337118,  1 /*forcé*/,  0.56040809, -0.15161053,  0.11156623,  0.90391943 ]
# -> win=61/159, good=151/159, score=0.224 x=[ 0.34337118  0.5         0.56040809 -0.15161053  0.11156623  0.90391943]
# Using this as initial value does not get better result --> we've got a keeper !
#
# [ 0.78234458,  0.79314842,  0.12955439,  0.38591404,  0.10160792, 0.38250826]
# win=58/159, good=150/159, score=0.244 x=[ 0.77028007  0.78318113  0.12257316  0.33847364  0.10345454  0.39324666]

# tests 7/6/15
# win=66/162, good=149/162, score=0.190 x=[ 0.21888181  1.46888588  0.56000048 -0.27271958  0.06774737  0.90789822]
# win=67/162, good=158/162, score=0.186 x=[ 0.55309081  1.29824525  0.28696765 -0.11984862  100.        0.        ]
# win=67/162, good=155/162, score=0.181 x=[ 0.68195435  1.26460976  0.23118265 -0.36108354  0.0677254   1.00050906]
# win=65/162, good=159/162, score=0.178 x=[ 1.12608295  1.35635675  0.31230791 -0.0118224   0.10017729  0.99999861]

init_values = [ 0.9,  1,  0.5, 0.3,  0.1,  1 ]

def f(args, tests):
    (final_coef_turn, final_turn_exp, final_distance_exp, final_coef_misc, final_score_v1_threshold, final_score_v1_coef) = args
    # cos/curve score: final_coef_cos, final_cos_exp, final_coef_curve, final_curve_exp
    # misc optim: (final_coef_turn, final_turn_exp, final_distance_exp, lazy_loop_bias, rt_score_coef, rt_score_coef_tip,
    # st5_score, tip_small_segment, turn_distance_score, ut_score) = args

    # final_turn_exp = 1 (override parameter value)
    # if final_coef_misc < 0: return -final_coef_misc
    # final_coef_misc = 0

    total = count = good = win = 0
    for test in tests.values():
        expected = test["ch"].get(test["expected"], None)
        if not expected: continue

        candidates = test["ch"].values()
        min_dist = min([c["distance_adj"] for c in candidates])
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
                            1 - 0.1 * (0.1 * (c["distance_adj"] - min_dist)) ** final_distance_exp) /
                            (final_coef_turn + 1))


        exp_score = expected["_score"]
        max_score = max([ c["_score"] for c in candidates if c["name"] != expected["name"] ] or [ exp_score ])
        x = (exp_score - max_score) * 10

        # value = x - x ** 2 if x < 0 else x / (1 + x)
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
# x0 = np.array([ 1.0, 0.5, 0.1, 1, 0.1, 0.5, 0.1, 0.5 ])
# x0 = np.array([ 1.0, 0.5, 0.1, 1, 0.1, 0.02 ])
# x0 = np.array([ 1.0, 1, 0.1, 1, 0.1, 0.02 ])
x0 = np.array(init_values)

print(f(x0, tests))

result = opt.fmin_bfgs(f, x0, args = (tests,), full_output = True)
print (result)
