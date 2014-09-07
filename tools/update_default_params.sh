#! /bin/sh -e

overwrite="$1"

mydir=`dirname "$0"`
head="$mydir/../curve/params.h"

tmp=`mktemp /tmp/okboard_params_h.XXXXXX`

$mydir/update_params.py < $head > $tmp

if cmp "$head" "$tmp" >/dev/null ; then
    rm -f "$tmp"
else
    echo "=== change to file "`realpath "$head"`" ==="
    diff -u "$head" "$tmp" || true
    if [ -n "$overwrite" ] ; then
	mv -fv "$tmp" "$head"
	echo "File updated"
    else
	rm -f "$tmp"
	echo "*** Add a nonempty parameter to update file !"
	exit 1
    fi
fi
     
echo "OK"
