#! /usr/bin/python3

import sys
import pickle
import matplotlib.pyplot as plt
from html import HTML  # https://pypi.python.org/pypi/html (it's great!)
import base64
import io
import numpy as np

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
    fig = plt.figure()

    for k1 in keys:
        x = kpos[k1][0]
        y = - kpos[k1][1]
        plt.plot([x], [y], 'o')
        plt.text(x + 10, y, k1, fontsize = 24)

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

    body.h5("Key: %s" % k)
    body.img(src = img64, border = '0')
    body.p

print(html)
