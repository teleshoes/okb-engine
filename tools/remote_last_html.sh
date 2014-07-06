#! /bin/sh

HOST=192.168.2.16 # <- my jolla usb ip address
d=`dirname "$0"`
tmp=`mktemp /tmp/curvekb.XXXXXX.html`
ssh nemo@$HOST 'tail -n 10 /tmp/curvekb.log | grep "^OUT:" | tail -n 1' | sed 's/^OUT:\ *//' | $d/jsonresult2html.py > $tmp && firefox "$tmp"

