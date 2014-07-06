#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys
import unicodedata
import re

from gribouille import *

t = LetterTree()

t.loadFromFile(sys.argv[1])

t.dump()
