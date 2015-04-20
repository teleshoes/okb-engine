#! /bin/bash -e

[ -f "$HOME/.okboard-test" ] && . $HOME/.okboard-test

outjson="$1"

d=`dirname "$0"`
host=${OKBOARD_REMOTE_HOST:-jolla} # <- my jolla hostname/ip address
tmp=`mktemp /tmp/curvekb.XXXXXX.html`
L="${OKBOARD_TEST_DIR:-/tmp}/curve.log"

ssh "nemo@$host" "( [ -f $L.bak ] && tail -n 10 $L.bak ; [ -f $L ] && tail -n 10 $L ) | grep \"^OUT:\" | tail -n 1" | sed 's/^OUT:\ *//' | (
    if [ -n "$outjson" ] ; then
	tee "$outjson"
    else
	cat
    fi
) | $d/jsonresult2html.py > $tmp && firefox "$tmp"

