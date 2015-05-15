#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" display curve matcher plugin output as HTML + images for "visual" evaluation """

import json
import sys
import re
from html import HTML  # https://pypi.python.org/pypi/html (it's great!)
try:
    from PIL import Image, ImageDraw
except:
    import Image, ImageDraw
import base64
import math
import io

txt = sys.stdin.read()
if txt[0] == '{':
    pass  # ok
else:
    txt = re.sub(r'^(.*\n|)Result:\s*', '', txt, flags = re.DOTALL)

js = json.loads(txt)
input = js['input']
keys = input['keys']
curves_mux = input['curve']
candidates = sorted(js['candidates'], key = lambda x: x['score'], reverse = True)

# calculate boundaries
xmin = min(k['x'] - k['w'] / 2 for k in keys)
xmax = max(k['x'] + k['w'] / 2 for k in keys)
ymin = min(k['y'] - k['h'] / 2 for k in keys)
ymax = max(k['y'] + k['h'] / 2 for k in keys)
width = xmax - xmin
height = ymax - ymin

# split curves
curve_count = max([ pt['curve_id'] for pt in curves_mux if "curve_id" if pt ]) + 1
curves = []
for i in range(0, curve_count):
    curves.append([ pt for pt in curves_mux if "end_marker" not in pt and pt.get("curve_id", 0) == i ])

# curve info
max_speed = max([ pt['speed'] for pt in curves_mux if 'speed' in pt ]) * 1.1
duration = max([ pt['t'] for pt in curves_mux if 't' in pt ] + [ 1 ])
for i in range(0, curve_count):
    curve = curves[i]
    last_pt = curve[0]
    length = 0

    def add_length(pt):
        global length, last_pt
        length += int(math.sqrt((pt['x'] - last_pt['x']) ** 2 + (pt['y'] - last_pt['y']) ** 2))
        pt['length'] = length
        last_pt = pt
        return pt
    curves[i] = list(map(add_length, curve))


# thingies
def clean_value(value):
    if value is False: return "-"
    if value is True: return "X"
    if value == 0: return "-"
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

def draw_key(draw, k, scale, key_color, text_color, draw_correction):
    draw.rectangle(coord( [ k['x'] - k['w'] / 2, k['y'] - k['h'] / 2, k['x'] + k['w'] / 2, k['y'] + k['h'] / 2], scale, border = -2),
                   fill = key_color)
    draw.text(coord( [ k['x'] - k['w'] / 2, k['y'] - k['h'] / 2 ], scale, offset = 4 ),
              k['k'],
              fill = text_color)
    if draw_correction and "corrected_x" in k:
        draw.line(coord( [ k['x'], k['y'], k['corrected_x'], k['corrected_y'] ], scale), fill = "#0000C0", width = 4)

def get_subscenarios_and_curves(scenario):
    ret = []
    if not scenario: return [ (None, c) for c in curves ]

    if "multi" in scenario:
        for i in range(0, len(curves)):
            ret.append( (scenario["scenarios"][i]["detail"], curves[i]) )
    else:
        ret = [ (scenario["detail"], curves[0]) ]
    return ret

def img2str(img):
    out = io.BytesIO()
    img.save(out, 'PNG')
    return "data:image/png;base64," + base64.b64encode(out.getvalue()).decode('UTF-8')


def mkimg(scale = 1, scenario = None):
    img = Image.new("RGB", (int(width * scale), int(height * scale)))
    draw = ImageDraw.Draw(img)

    for k in keys:
        draw_key(draw, k, scale, '#80C0FF', '#000000', not scenario)

    for curve in curves:
        lastx, lasty = None, None
        c = 0
        if len(curve) < 2:
            x = int(curve[0]['x'] * scale) - xmin
            y = int(curve[0]['y'] * scale) - ymin
            for i in [0, 1]:
                r = int(scale * (10 - 4 * i))
                draw.ellipse((x - r, y - r, x + r, y + r), fill="#E08000" if i else "#FFC000")

        for pt in curve:
            x = int(pt['x'] * scale) - xmin
            y = int(pt['y'] * scale) - ymin
            if lastx is not None:
                if pt['sharp_turn'] and not scenario:
                    s = int(50 * scale)
                    if 'normalx' in pt:
                        draw.line((x, y, x + int(0.5 * pt['normalx'] * scale), y + int(0.5 * pt['normaly'] * scale)), fill="#008000", width = int(2 * scale))
                    draw.line((x - s, y, x + s, y), fill="#C00000", width = int(2 * scale))
                    draw.line((x, y - s, x, y + s), fill="#C00000", width = int(2 * scale))

                if c % 10 == 0 and not scenario:
                    rs = int(5 * scale)
                    draw.rectangle((lastx - rs, lasty - rs, lastx + rs, lasty + rs), fill='#0000B0')
                c += 1

                draw.line((lastx, lasty, x, y), fill=[ '#FFC000', '#E08000' ][ c % 2 if not scenario else 0 ], width = int(6 * scale))
            lastx, lasty = x, y

    if scenario:
        draw_scenarios = [ ]
        draw_scenarios = get_subscenarios_and_curves(scenario)

        for draw_scenario, curve in draw_scenarios:
            col = '#FF0000'
            lastx, lasty, lastxc, lastyc = None, None, None, None
            for step in draw_scenario:
                key = [ k for k in keys if k["k"] == step["letter"] ][0]
                index = step["index"]
                (x1, y1, x2, y2) = coord([ key.get('corrected_x', key['x']), key.get('corrected_y', key['y']), curve[index]['x'], curve[index]['y'] ], scale)
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

    return img2str(img)

def get_letter(scenario, index):
    l = [ s["letter"] for s in scenario if s["index"] == index ]
    return l[0] if len(l) else None

def mkxygraph(size = 120, scenario = None):
    img = Image.new("RGB", (size, size), color = '#C0C0C0')
    draw = ImageDraw.Draw(img)

    draw.line((0, size / 2, size - 1, size / 2), fill='#0000C0', width = 2)
    draw.line((size / 2, 0, size / 2, size - 1), fill='#0000C0', width = 2)

    id = 0
    draw_scenarios = get_subscenarios_and_curves(scenario)
    for draw_scenario, curve in draw_scenarios:
        color = [ '#C04000', '#0080C0' ][id % 2]
        id += 1

        lastdx, lastdy = None, None
        c = 0
        if len(curve) < 3: continue
        for i in range(0, len(curve)):
            i1, i2 = max(0, i - 1), min(len(curve) - 1, i + 1)
            dx = curve[i2]['x'] - curve[i1]['x']
            dy = curve[i2]['y'] - curve[i1]['y']
            speed = curve[i]['speed']
            l = math.sqrt(dx * dx + dy * dy)
            dx = size * (0.5 + 0.5 * speed * dx / l / max_speed)
            dy = size * (0.5 + 0.5 * speed * dy / l / max_speed)
            if draw_scenario:
                letter = get_letter(draw_scenario, c)
                if letter:
                    draw.rectangle((dx - 6, dy - 6, dx + 6, dy + 6), fill='#008000')
                    draw.text((dx + 7, dy + 7), letter, fill='#804000')
            c += 1
            if not c % 10 and not draw_scenario: draw.rectangle((dx - 6, dy - 6, dx + 6, dy + 6), fill='#008000')
            if lastdx: draw.line((lastdx, lastdy, dx, dy), fill=color, width = 4)
            lastdx, lastdy = dx, dy

    return img2str(img)

def mkspeedgraph(sx = 240, sy = 120, scenario = None):
    img = Image.new("RGB", (sx, sy), color = '#C0C0C0')
    draw = ImageDraw.Draw(img)

    id = 0
    draw_scenarios = get_subscenarios_and_curves(scenario)
    for draw_scenario, curve in draw_scenarios:
        color = [ '#C04000', '#0080C0' ][id % 2]

        lastx, lasty = None, None
        c = 0
        if len(curve) < 2:
            x = int((sx - 1) * curve[0]['t'] / duration)
            draw.rectangle((x - 4, int(sy / 4), x + 4, int(3 * sy / 4)), fill=color)

        for pt in curve:
            x = (sx - 1) * pt['t'] / duration
            y = sy - 1 - (sy - 1) * pt['speed'] / max_speed
            if draw_scenario:
                letter = get_letter(draw_scenario, c)
                if letter:
                    draw.line((x, 0, x, sy - 1), fill='#008000', width = 2)
                    draw.text((x + (2 if x == 0 else -2 - draw.textsize(letter)[0]), 2 + 9 * id), letter, fill='#008000')
            c += 1
            if not c % 10 and not draw_scenario: draw.line((x, 0, x, sy - 1), fill='#008000', width = 2)
            if lastx is not None: draw.line((lastx, lasty, x, y), fill=color, width = 4)
            lastx, lasty = x, y

        id += 1

    return img2str(img)

def mux_scenario(scenario):
    if "multi" not in scenario: return scenario
    detail = []
    for d in scenario["detail"]:
        curve_id = d["curve_id"]
        index = d["index"]

        sub_detail = scenario["scenarios"][curve_id]["detail"][index]
        sub_detail["cid:pos"] = "%d:%d" % (curve_id, index)
        detail.append(sub_detail)

    return dict(detail = detail)

# generate html
title = 'Okboard result ' + input['datetime']

html = HTML()
html.head.title(title)
body = html.body

body.h3(title)
body.meta(charset = "UTF-8")
body.img(src = mkimg(scale = 1.4), border = '0')
body.p()
body.img(src = mkxygraph(size = 250), border = '0')
body.span(" ")
body.img(src = mkspeedgraph(sx = 500, sy = 250), border = '0')

body.p(font_size="-2").i("Word tree file: %s" % input["treefile"]).br. \
    i("Time: %d ms - Matches: %d - Nodes: %d - Points: %d - Draw time: %d ms [%s] - Build: [%s]" %
      (js["stats"]["time"], len(candidates), js["stats"]["count"], len(curve), curve[-1]["t"] - curve[0]["t"], js["ts"],
       js.get("build", "unknown")))


# params
params = sorted(input["params"].items())
while params:
    t = body.table(border = "1")
    hr = t.tr
    tr = t.tr
    for p, v in params[0:30]:
        td = hr.td(align = "center", bgcolor="#C0FFC0").font(size = "-2")
        for part in p.split('_'):
            td += part
            td.br
        tr.td(align = "center").font(clean_value(v), size = "-2")
    params = params[30:]

body.p

# curve
count = 0
for curve in curves:
    body.p("Curve: #%d" % count)
    count += 1
    c = 0
    curve_ = curve
    while curve_:
        curve1 = curve_[0:45]
        curve_ = curve_[45:]
        t = body.table(border = "1")
        li = t.tr(align = "center").td(bgcolor = "#C0FFC0").font('#', size = "-2")
        for i in range(0, len(curve1)):
            li.td(bgcolor="#E0E0E0").font(str(c), size = "-2")
            c += 1
        for lbl in ['x', 'y', 't', 'speed', 'turn_angle', 'turn_smooth', 'sharp_turn', 'length']:
            li = t.tr(align = "center").td.font(lbl, size = "-2", bgcolor="#C0FFC0")
            for pt in curve1:
                col = ""
                if pt.get("dummy", 0): col = "#FFC040"
                if lbl == "sharp_turn" and pt.get("sharp_turn"): col = "#FF8080"
                li.td(bgcolor=col).font(clean_value(pt[lbl]), size = "-2")

# overview
if candidates:
    body.p
    multi = candidates[0].get("multi", None)
    s0avg = candidates[0]["scenarios"][0]["avg_score"] if multi else candidates[0]["avg_score"]
    scores = sorted(s0avg.keys())

    t = body.table(border = "1")
    hr0 = t.tr
    hr = t.tr
    if multi:
        hr0.td(bgcolor="#C0FFC0", colspan = "8", align = "center").font("Multi-scenario", size="-2")
        for col in [ "Name", "ID", "Score", "Err", "Gd", "Dist", "Dist-2", "Words" ]:
            hr.td(align = "center", bgcolor="#C0FFC0").font(col, size="-2")

    for col in ["Name", "Score", "Min", "Err", "Gd", "Dist", "Dist-2", "Sc-0", "Sc-1", "Words" ]:
        hr0.td(bgcolor="#C0FFC0", rowspan="2").font(col, size="-2")
    for typ in [ "avg_score", "min_score" ]:
        hr0.td(bgcolor="#C0FFC0", colspan = str(len(scores)), align = "center").font(typ, size="-2")
        for sc in scores: hr.td(align = "center", bgcolor="#C0FFC0").font(re.sub(r'^score_', '', sc), size="-2")
    hr0.td(bgcolor="#C0FFC0", rowspan="2").font("Class", size="-2")
    for scenario in candidates:
        sub_scenarios = scenario["scenarios"] if multi else [ scenario ]

        first = True
        for sub_scenario in sub_scenarios:
            tr = t.tr
            if multi and first:
                first = False
                for col in [ scenario["name"], scenario["id"], scenario["score"],
                             scenario["error"], scenario["good"],
                             scenario.get("distance", 0), int(scenario.get("distance", 0) / math.sqrt(len(scenario["name"]))),
                             scenario.get("words", "-") ]:
                    tr.td(rowspan = str(len(sub_scenarios)), align = "center").font(clean_value(col), size="-2")

            for col in [ sub_scenario["name"], sub_scenario["score"], sub_scenario["min_total"], sub_scenario.get("error", ""),
                         sub_scenario.get("good", ""), int(sub_scenario.get("distance", 0)),
                         int(sub_scenario.get("distance", 0) / math.sqrt(len(sub_scenario["name"]))),
                         sub_scenario.get("score_std", ""), sub_scenario.get("score_v1", ""),
                         sub_scenario.get("words", "-") if not multi else "n/a" ]:
                tr.td(align = "center").font(clean_value(col), size="-2")
            for typ in [ "avg_score", "min_score" ]:
                for sc in scores: tr.td(align = "center").font(clean_value(sub_scenario[typ][sc]), size="-2")
            tr.td(align = "center").font(str(sub_scenario.get("class", "?")) + ("*" if sub_scenario.get("star") else ""), size="-2")

# matches
n = 1
for scenario in candidates:
    body.h5('Match #%d : %s [score=%.3f]' % (n, scenario['name'], scenario['score']))

    t0 = body.table(border = "0").tr

    t0.td.img(src = mkimg(scale = 0.75, scenario = scenario), border = '0')

    first = True
    t = t0.td.table(border = "1")
    total = dict()
    for letter_info in mux_scenario(scenario)["detail"]:
        if first:
            hr = t.hr
            for k in sorted(letter_info.keys()): hr.td(bgcolor="#C0FFC0", align = "center").font(k, size="-2")
        tr = t.tr

        color = ""
        cp = letter_info.get("cid:pos", None)
        if cp:
            cid = int(cp.split(":")[0]) % 4
            if cid: color = '#%06x' % (0xFFFF00 + 0x000001 * (255 - 45 * cid))

        for k in sorted(letter_info.keys()):
            tr.td(clean_value(letter_info[k]), align = "center", bgcolor=color)
            if k.startswith("score_"):
                if letter_info[k]:
                    if k not in total: total[k] = (0, 0)
                    total[k] = tuple(map(sum, zip(total[k], (1.0 * letter_info[k], 1))))
        first = False

    tr = t.tr
    for k in sorted(letter_info.keys()):
        tr.td(clean_value(total[k][0] / total[k][1]) if k in total else "", bgcolor="#C0C0C0", align = "center")
    n += 1

    t0.td.img(src = mkxygraph(size = 300, scenario = scenario), border = '0')
    t0.td.img(src = mkspeedgraph(sx = 400, sy = 300, scenario = scenario), border = '0')


body.hr
body.p.pre(json.dumps(input))

print(html)
