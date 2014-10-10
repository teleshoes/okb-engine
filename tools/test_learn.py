#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys, os
import getopt
import time

mydir = os.path.dirname(os.path.abspath(__file__))
libdir = os.path.join(mydir, '..')
sys.path.insert(0, libdir)

save = False
loop = False
opts, args =  getopt.getopt(sys.argv[1:], 'wl')
for o, a in opts:
    if o == "-w":
        save = True
    elif o == '-l':
        loop = True
    else:
        print("Bad option: %s" % o)
        exit(1)

import predict

db_file = args[0]

p = predict.Predict(cfvalues = dict(verbose = True))
p.set_dbfile(db_file)
p.load_db()

text = sys.stdin.readlines()

def get_candidates(p, context, cache):
    context = context[-2:]
    key = ':'.join(context)
    if key in cache: return set(cache[key])

    conn = p.db.conn
    curs = conn.cursor()

    sql = """
    select w1.word
    from words w1, words w2, words w3, grams g
    where g.id1 = w1.id and g.id2 = w2.id and g.id3 = w3.id
    and w2.word = ? and w3.word = ?
    and w1.word != '#TOTAL' and w1.word != '#START'
    order by stock_count desc
    limit 3
    """

    curs.execute(sql, (context[1], context[0]))
    rows = curs.fetchall()
    result = set([ x[0] for x in rows ][0:3])
    cache[key] = result
    return set(result)

def guess_and_learn(text, p, cache, compare = [ ], commit = False):
    score = count = lcount = 0
    for line in text:
        lcount += 1
        print(">>> line #%d: %s" % (lcount, line))
        words = line.strip().split(' ')
        context = [ '#NA', '#NA' ]
        if words and words[0] == '#ERR':
            words.pop(0)
        else:
            context += [ '#START' ]

        real_text = ""
        p.update_surrounding("", 0)

        for word in words:
            # guess words (input = 3 better choices from predict DB + expected word)
            candidates = get_candidates(p, context, cache)
            candidates.add(word)  # expected word
            print(" [Word: %s] context = %s - candidates = %s" % (word, context, candidates))
            choice = p.guess([ (x, 1.0, 0, False, '=') for x in candidates ])

            ok = (word == choice)
            if ok: score += 1

            while len(compare) < count + 1: compare.append(None)
            change = "[=]"
            if compare[count] is None:
                change = "[N]"
            elif ok != compare[count]:
                change = "[+]" if ok else "[-]"
            compare[count] = ok

            count += 1
            print(" ---> word #%d: expected: %s - actual: %s %s\n" % (count, word, choice, change))

            context.append(word)
            context = context[-3:]

            # update context
            real_text += word + " "
            p.update_surrounding(real_text, len(real_text))

        if lcount % 10 == 0 and commit: p._commit_learn(commit_all = True)


    if commit: p._commit_learn(commit_all = True)
    return 1.0 * score / count


it = 0
now = time.time()
compare = [ ]
while True:
    it += 1
    print ("""
==========================
Iteration %d
==========================
    """ % it)

    cache = dict()
    score = guess_and_learn(text, p, cache, compare = compare, commit = save)
    print("\n********* Iteration #%d: Final score = %.2f ***********\n" % (it, score))

    if not loop: break
    now += 86400
    p._mock_time(now)
