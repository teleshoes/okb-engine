#! /bin/bash -x

test="$1"
edit="$2"
if [ ! -f "$test" ] ; then echo "usage: $0 <json test file> [<editor for log file>]" ; exit 1; fi
dir=`dirname "$0"`"/.."
dir=`readlink -f "$dir"`
name=`basename "$test" .json`

lang="en"
if echo "$name" | egrep '^[a-z][a-z]\-' >/dev/null ; then
    lang=`echo "$name" | head -n 1 | cut -c 1,2`
fi

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$dir/curve/build"
tmp=`mktemp /tmp/$name.XXXXXX.json`
html=`mktemp /tmp/curvekb.$name.XXXXXX.html`
if $dir/cli/build/cli ${CLI_OPTS} -d "$dir/db/$lang.tre" "$test" 2>&1 | tee $tmp | grep -i "^Result:" | tail -n 1 | sed 's/^Result:\ *//' | $dir/tools/jsonresult2html.py > $html; then
    xdg-open "$html"
else
    echo "FAILED!"
fi
echo "Log file: file://$tmp"
if [ -n "$edit" ] ; then
    edit=`which "$edit" 2>/dev/null`
    [ -n "$edit" ] && $edit $tmp
fi

