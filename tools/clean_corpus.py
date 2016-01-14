#! /usr/bin/python3
# -*- coding: utf-8 -*-

# corpus file cleaner
# check output by hand to be sure :-)

import sys, re

f = sys.stdin.buffer

buf = f.readline()
while buf:
    line = buf.decode('utf-8', 'ignore')  # ignore UTF-8 encoding errors
    line = re.sub('\r', '', line)

    # convert some UTF-8 character to ASCII equivalent (very incomplete)
    line = re.sub(r'\u2013', '-', line)  # hyphen
    line = re.sub(r'[\u2019\u2018]', "'", line)  # quote
    line = re.sub(r'[\u201C\u201D]', '"', line)  # double-quote
    line = re.sub(r'[\u2026\u22C5]', '.', line)  # ellipsis & dots

    line = re.sub(r'[^\ \!-\~\t\r\n\'\u0080-\u023F\u20AC]', ' ', line)

    print(line, end = '')

    buf = f.readline()
