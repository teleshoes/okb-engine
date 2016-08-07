#! /usr/bin/python3

import sys
import pickle
import matplotlib.pyplot as plt
from html import HTML  # https://pypi.python.org/pypi/html (it's great!)
import base64
import io
import numpy as np
import math

db = pickle.load(open(sys.argv[1], "rb"))

keys = set([ item["key"] for item in db ])

kpos = dict()
for item in db:
    kpos[item["key"]] = (item["kx"], item["ky"])

title = "OKBoard key errors"

html = HTML()
html.head.title(title)
body = html.body

body.h3(title)
body.meta(charset = "UTF-8")

for k in sorted(keys):

    # graph 1
    fig = plt.figure()

    for k1 in keys:
        x_ = kpos[k1][0]
        y_ = - kpos[k1][1]
        plt.plot([x_], [y_], 'o')
        plt.text(x_ + 10, y_, k1, fontsize = 24)

    X = [ item["mx"] for item in db if item["key"] == k ]
    Y = [ - item["my"] for item in db if item["key"] == k ]

    plt.plot(X, Y, ".")

    kx = kpos[k][0]
    ky = - kpos[k][1]
    ax = np.average(X)
    ay = np.average(Y)

    plt.plot([kx, ax], [ky, ay], color='blue', linestyle='-', linewidth=5)

    plt.tight_layout()
    plt.grid(True)
    plt.axis('equal')

    out = io.BytesIO()
    plt.savefig(out, format = 'PNG')
    plt.close(fig)
    plop = base64.b64encode(out.getvalue())
    img64 = "data:image/png;base64," + str(plop, 'utf-8')

    # graph 2
    fig = plt.figure()
    X2 = []
    Y2 = []
    for item in [ i for i in db if i["key"] == k ]:
        if not item["speed"] or item["speed"] > 2500: continue
        mx, my = item["mx"], - item["my"]
        v1 = [ ax - kx, ay - ky ]
        v2 = [ mx - kx, my - ky ]
        l1 = np.linalg.norm(v1)
        l2 = np.linalg.norm(v2)
        X2.append(np.dot(v1, v2) / l1 / l2 if l1 and l2 else 0)
        Y2.append(item["speed"])

    plt.plot(X2, Y2, ".")
    fit = np.polyfit(X2, Y2, 1)
    fit_fn = np.poly1d(fit)
    plt.plot(X2, fit_fn(X2), '--k')
    out = io.BytesIO()
    plt.savefig(out, format = 'PNG')
    plt.close(fig)
    plop = base64.b64encode(out.getvalue())
    img64b = "data:image/png;base64," + str(plop, 'utf-8')

    # stats
    D = [ math.sqrt((x - ax) ** 2 + (y - ay) ** 2) for (x, y) in zip(X, Y) ]
    body.h5("Key: %s" % k)
    body.br("Distance (percentiles) : 90: %.2f, 95: %.2f, 98: %.2f, max: %.2f" %
            (np.percentile(D, 90), np.percentile(D, 95), np.percentile(D, 98), max(D)))
    body.br("Center: (%d, %d), count: %d, distance-avg: %d" % (ax - kx, ay - ky, len(X), np.average(D)))

    ratio = lambda p: ((np.percentile(X, 100 - p / 2) - np.percentile(X, p / 2)) /
                       (np.percentile(Y, 100 - p / 2) - np.percentile(Y, p / 2)))
    body.br("Ratio X/Y: 95: %.2f, 98: %.2f, max: %.2f" % (ratio(5), ratio(2), ratio(0)))

    sx = np.std(X)
    sy = np.std(Y)
    body.br("Stddev X: %.2f, Y: %.2f, D: %2.f" % (sx, sy, math.sqrt(sx ** 2 + sy ** 2)))

    body.img(src = img64, border = '0')
    body.img(src = img64b, border = '0')
    body.p
    body.hr


print(html)
