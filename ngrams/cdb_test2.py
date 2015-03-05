#! /usr/bin/python3
# -*- coding: utf-8 -*-

import cdb
import tempfile

(h, tmp) = tempfile.mkstemp()

cdb.load(tmp)

cdb.set_string("s1", "plop")
cdb.set_gram("1:1:1", 1.0, 0.0, 1)
cdb.set_gram("1:1:2", 10.0, 0.0, 100)
cdb.set_words("toto", dict(toto1 = (1, 0), toto2 = (2, 0)))

cdb.purge_grams(5.0, 10)


l = cdb.get_keys()
assert(set(l) == set(["s1", "1:1:2", "toto"]))

w = cdb.get_word_by_id(2)
assert(w == "toto2")
