#! /usr/bin/python3
# -*- coding: utf-8 -*-

from gribouille import *

t = LetterTree()

t.beginLoad()

t.addWord("blabla", "1")
t.addWord("bloblo", "2")
t.addWord("pipo", "3")

t.endLoad()

t.dump()

def rec(x, p = ""):
    pl = x.getPayload()
    print("test:", p, ("'%s'" % pl) if pl else "")

    for child in x.getChilds():
        print("child", p + child.letter)
        rec(child, p + child.letter)

rec(t.getRoot())

