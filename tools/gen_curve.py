#! /usr/bin/python3
# -*- coding: utf-8 -*-

# generate "plausible" curves in order to produce new test cases automatically
# (we need a huge amount of these for prediction engine tuning & testing)
#

import sys, os, re, json, math
import unicodedata
import getopt
import random

import optim

#  --- Bézier curve stuff (from https://stackoverflow.com/questions/12643079/bézier-curve-fitting-with-scipy)
import numpy as np
from scipy.misc import comb

def bernstein_poly(i, n, t):
    """
     The Bernstein polynomial of n, i as a function of t
    """

    return comb(n, i) * ( t ** (n - i) ) * (1 - t) ** i


def bezier_curve(points, nTimes = 1000):
    """
       Given a set of control points, return the
       bezier curve defined by the control points.

       points should be a list of lists, or list of tuples
       such as [ [1,1],
                 [2,3],
                 [4,5], ..[Xn, Yn] ]
        nTimes is the number of time steps, defaults to 1000

        See http://processingjs.nihongoresources.com/bezierinfo/
    """

    nPoints = len(points)
    xPoints = np.array([p[0] for p in points])
    yPoints = np.array([p[1] for p in points])

    t = np.linspace(0.0, 1.0, nTimes)

    polynomial_array = np.array([ bernstein_poly(i, nPoints - 1, t) for i in range(0, nPoints)   ])

    xvals = np.dot(xPoints, polynomial_array)
    yvals = np.dot(yPoints, polynomial_array)

    return xvals, yvals
# --- /Bézier curve stuff

# --- vector utils
def v_length(v):
    return np.sqrt((v * v).sum())

def v_angle(v1, v2):
    cos_angle = np.dot(v1, v2) / v_length(v1) / v_length(v2)
    if abs(cos_angle) > 1: cos_angle = math.copysign(1, cos_angle)  # avoid NaN on rounding error
    angle = np.arccos(cos_angle) * 180 / np.pi
    if np.cross([0] + v1.tolist(), [0] + v2.tolist())[0] < 0:
        angle = -angle
    return angle

def v_rotate(v, angle):
    angle *= np.pi / 180
    return np.dot(np.array([ [ math.cos(angle), -math.sin(angle) ],
                             [ math.sin(angle), math.cos(angle) ] ]), v)

def dist(p1, p2):
    x = p2[0] - p1[0]
    y = p2[1] - p1[1]
    return math.sqrt(x * x + y * y)
# --- /vector utils

def word2keys(word):
    letters = ''.join(c for c in unicodedata.normalize('NFD', word) if unicodedata.category(c) != 'Mn')
    letters = re.sub(r'[^a-z]', '', letters.lower())
    letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
    return letters

def gen_curve(word, js, error = 0, plot = False, curviness = 150, lang = "en", retry = 10, out_file = None, verbose = True, max_err = None):
    if verbose: print("=== [%s] curviness=%d error=%d lang=%s" % (word, curviness, error, lang))

    letters = word2keys(word)

    keys = js["keys"]

    class Point:
        def __init__(self, x, y):
            self.pt = np.array([x, y])

    if plot:
        import matplotlib.pyplot as plt
        plt.figure()
        plt.gca().invert_yaxis()


    while True:  # in case of retries

        points = []
        for letter in letters:
            key = [ k for k in keys if k["k"] == letter ][0]
            point = Point(float(key["x"]), float(key["y"]))
            if error:
                point.pt[0] += (random.random() * 2 - 1) * error
                point.pt[1] += (random.random() * 2 - 1) * error
            points.append(point)

        n = len(points)
        for i in range(1, n - 1):
            points[i].turn = v_angle(points[i].pt - points[i - 1].pt,
                                     points[i + 1].pt - points[i].pt)

            points[i].st = (abs(points[i].turn) > 165)


        curve = []
        lastp = None
        t = 0

        for i in range(0, n - 1):
            pt1 = points[i].pt
            pt2 = points[i + 1].pt
            segment = pt2 - pt1
            l = v_length(segment)
            segment /= l

            turn1 = turn2 = 0
            if i > 0:
                if points[i].st:
                    turn1 = (math.copysign(180, -points[i].turn) + points[i].turn) / 2
                else:
                    turn1 = points[i].turn / 2

            if i < n - 2:
                if points[i + 1].st:
                    turn2 = (math.copysign(180, -points[i + 1].turn) + points[i + 1].turn) / 2
                else:
                    turn2 = points[i + 1].turn / 2

            if i == 0: turn1 = turn2
            elif i == n - 2: turn2 = turn1

            t1 = t2 = min(l * 0.3, curviness)
            ctr = [ pt1,
                    pt1 + v_rotate(segment, - turn1) * t1,
                    pt2 - v_rotate(segment, turn2) * t2,
                    pt2 ]
            bzx, bzy = bezier_curve(ctr, nTimes = 50)

            if plot:
                plt.plot([ c[0] for c in ctr ], [ c[1] for c in ctr])
                plt.plot(bzx, bzy)

            for p in reversed(list(zip(bzx, bzy))):  # don't know why the curve is reversed
                if lastp is None or dist(p, lastp) >= 15:
                    curve.append(dict(x = int(p[0]), y = int(p[1]), t = t))
                    t += 10
                    lastp = p

        js["curve"] = curve
        js["params"] = dict([ (x, y["value"]) for x, y in optim.params.items() ])

        js["params"]["thumb_correction"] = 0  # this code has no thumb :-)

        if out_file:
            open(out_file, "w").write(json.dumps(js))

        (json_out, err, code, cmd) = optim.run1(json.dumps(js), lang, full = False)

        if code: raise Exception("Curve plugin crashed!")

        json_out = json_out.decode(encoding = 'UTF-8')
        i = json_out.find('Result: {')
        if i > -1: json_out = json_out[i + 8:]
        result = json.loads(json_out)

        candidates = []
        for c in result["candidates"]:
            allwords = c["words"].split(',')
            for w in allwords:
                c1 = dict(c)
                c1["word"] = c["name"] if w == '=' else w
                candidates.append(c1)

        candidates.sort(key = lambda x: x["score"], reverse = True)

        w2s = dict([ (c["word"], c["score"]) for c in candidates ])
        ok = (word in w2s and (not max_err or w2s[word] >= max(w2s.values()) - max_err))

        if plot or ok or not retry: break

        if verbose: print("Retry: %d\n" % retry)

        retry -= 1
        if retry == 0: raise Exception("Retry count exceeded")
        if retry < 10:
            curviness = int(curviness * 0.8)
            error = int(error * 0.8)

    if verbose:
        for c in candidates:
            print(c["word"], c["name"], c["score"], c["star"], c["class"], c["words"])

    if plot:
        plt.plot([ p.pt[0] for p in points], [ p.pt[1] for p in points], "ro")
        plt.show()

    return candidates

if __name__ == "__main__":
    error = 0
    plot = False
    curviness = 150
    lang = "en"
    retry = 10
    out_file = None
    max_err = None
    opts, args =  getopt.getopt(sys.argv[1:], 'e:pc:s:l:r:o:a:')
    for o, a in opts:
        if o == "-e":  # error
            error = int(a)
        elif o == "-p":  # plot to screen
            plot = True
        elif o == "-s":  # seed
            random.seed(int(a))
        elif o == "-c":
            curviness = int(a)
        elif o == "-l":
            lang = a
        elif o == "-r":
            retry = int(a)
        elif o == "-o":
            out_file = a
        elif o == "-a":
            max_err = float(a)
        else:
            print("Bad option: %s" % o)
            exit(1)

    jsonfile = args[0]
    word = args[1]

    js = json.loads(open(jsonfile, "r").read())
    os.chdir(os.path.dirname(__file__))

    gen_curve(word, js, error = error, plot = plot, curviness = curviness, lang = lang, retry = retry, out_file = out_file, max_err = max_err)
