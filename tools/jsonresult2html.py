#! /usr/bin/python2
# -*- coding: utf-8 -*-

""" display curve matcher plugin output as HTML + images for "visual" evaluation """

import json
import sys
import re
from html import HTML  # https://pypi.python.org/pypi/html (it's great!)
import Image, ImageDraw
import base64
import StringIO

txt = sys.stdin.read()
if txt[0] == '{':
    pass  # ok
else:
    txt = re.sub(r'^(.*\n|)Result:\s*', '', txt, flags = re.DOTALL)

js = json.loads(txt)
input = js['input']
keys = input['keys']
curve = input['curve']
candidates = sorted(js['candidates'], key = lambda x: x['score'], reverse = True)

# calculate boundaries
xmin = min(k['x'] - k['w'] / 2 for k in keys)
xmax = max(k['x'] + k['w'] / 2 for k in keys)
ymin = min(k['y'] - k['h'] / 2 for k in keys)
ymax = max(k['y'] + k['h'] / 2 for k in keys)
width = xmax - xmin
height = ymax - ymin

# thingies
def clean_value(value):
    if value is False: return "-"
    if value is True: return "X"
    if str(value).find('.') > -1:
        try: return "%.3f" % float(value)
        except: pass
    return str(value)

# image drawing
def coord(l, scale, border = 0, offset = 0):
    result = []
    for i in range(0, len(l)):
        x = int((l[i] - (xmin if not i % 2 else ymin)) * scale)
        if i < len(l) / 2:
            x -= border
        else:
            x += border
        x += offset
        result.append(x)
    return tuple(result)

def draw_key(draw, k, scale, key_color, text_color):
    draw.rectangle(coord( [ k['x'] - k['w'] / 2, k['y'] - k['h'] / 2, k['x'] + k['w'] / 2, k['y'] + k['h'] / 2], scale, border = -2),
                   fill = key_color)
    draw.text(coord( [ k['x'] - k['w'] / 2, k['y'] - k['h'] / 2 ], scale, offset = 4 ),
              k['k'],
              fill = text_color)

def mkimg(scale = 1, scenario = None):
    img = Image.new("RGB", (int(width * scale), int(height * scale)))
    draw = ImageDraw.Draw(img)

    for k in keys:
        draw_key(draw, k, scale, '#80C0FF', '#000000')

    lastx, lasty = None, None
    c = 0
    for pt in curve:
        x = int(pt['x'] * scale) - xmin
        y = int(pt['y'] * scale) - ymin
        if lastx is not None:
            if pt['sharp_turn'] and not scenario:
                s = int(50 * scale)
                draw.line((x, y, x + int(50.0 * pt['normalx'] * scale), y + int(50.0 * pt['normaly'] * scale)), fill="#008000", width = int(2 * scale))
                draw.line((x - s, y, x + s, y), fill="#C00000", width = int(2 * scale))
                draw.line((x, y - s, x, y + s), fill="#C00000", width = int(2 * scale))

            if c % 10 == 0 and not scenario:
                rs = int(5 * scale)
                draw.rectangle((lastx - rs, lasty - rs, lastx + rs, lasty + rs), fill='#0000B0')
            c += 1

            draw.line((lastx, lasty, x, y), fill=[ '#FFC000', '#E08000' ][ c % 2 if not scenario else 0 ], width = int(6 * scale))
        lastx, lasty = x, y

    if scenario:
        col = '#FF0000'
        lastx, lasty, lastxc, lastyc = None, None, None, None
        for step in scenario["detail"]:
            key = [ k for k in keys if k["k"] == step["letter"] ][0]
            index = step["index"]
            (x1, y1, x2, y2) = coord([ key['x'], key['y'], curve[index]['x'], curve[index]['y'] ], scale)
            if lastx is not None:
                draw.line((lastx, lasty, x1, y1), fill='#000080', width = int(3 * scale))
            if lastxc is not None:
                draw.line((lastxc, lastyc, x2, y2), fill = '#008000', width = int(3 * scale))
            lastx, lasty = x1, y1
            lastxc, lastyc = x2, y2
            draw.line((x1, y1, x2, y2),
                      fill = col,
                      width = int(6 * scale))
            draw.rectangle((x1 - 4, y1 - 4, x1 + 4, y1 + 4), fill = col)
            draw.rectangle((x2 - 4, y2 - 4, x2 + 4, y2 + 4), fill = col)


    out = StringIO.StringIO()
    img.save(out, 'PNG')
    return "data:image/png;base64," + base64.b64encode(out.getvalue())

# generate html
title = 'Okboard result ' + input['datetime']

html = HTML()
html.head.title(title)
body = html.body
body.h3(title)

body.img(src = mkimg(scale = 1.4), border = '0')
body.p(font_size="-2").i("Word tree file: %s" % input["treefile"]).br. \
    i("Time: %d ms - Matches: %d - Nodes: %d - Points: %d - Draw time: %d ms [%s]" %
      (js["stats"]["time"], len(candidates), js["stats"]["count"], len(curve), curve[-1]["t"] - curve[0]["t"], js["ts"]))

# params
params = input["params"]
t = body.table(border = "1")
hr = t.hr
tr = t.tr
for p, v in sorted(params.items()):
    td = hr.td(align = "center", bgcolor="#C0FFC0").font(size="-2")
    for part in p.split('_'):
        td += part
        td.br
    tr.td(clean_value(v), align = "center")

body.p

# curve
c = 0
curve_ = curve
while curve_:
    curve1 = curve_[0:30]
    curve_ = curve_[30:]
    t = body.table(border = "1")
    li = t.tr(align = "center").td('#', bgcolor = "#C0FFC0")
    for i in range(0, len(curve1)):
        c += 1
        li.td(str(c), bgcolor="#E0E0E0")
    for lbl in ['x', 'y', 't', 'speed', 'turn_angle', 'turn_smooth', 'sharp_turn']:
        li = t.tr(align = "center").td(lbl, bgcolor="#C0FFC0")
        for pt in curve1:
            li.td(clean_value(pt[lbl]))

# matches
n = 1
for scenario in candidates:
    body.h5('Match #%d : %s [score=%.3f]' % (n, scenario['name'], scenario['score']))

    t0 = body.table(border = "0").tr

    t0.td.img(src = mkimg(scale = 0.75, scenario = scenario), border = '0')

    first = True
    t = t0.td.table(border = "1")
    total = dict()
    for letter_info in scenario["detail"]:
        if first:
            hr = t.hr
            for k in sorted(letter_info.keys()): hr.td(bgcolor="#C0FFC0", align = "center").font(k, size="-2")
        tr = t.tr
        for k in sorted(letter_info.keys()):
            tr.td(clean_value(letter_info[k]), align = "center")
            if k.startswith("score_"):
                if str(letter_info[k]) != '0':
                    if k not in total: total[k] = (0, 0)
                    total[k] = tuple(map(sum, zip(total[k], (1.0 * letter_info[k], 1))))
        first = False

    tr = t.tr
    for k in sorted(letter_info.keys()):
        tr.td(clean_value(total[k][0] / total[k][1]) if k in total else "", bgcolor="#C0C0C0", align = "center")
    n += 1

body.hr
body.p.pre(json.dumps(input))

print unicode(html)
