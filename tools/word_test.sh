#! /bin/sh -xe
# just check test matching with expected word 
# (generater far less output)

test="$1"
if [ ! -f "$test" ] ; then echo "usage: $0 <json test file>" ; exit 1; fi
dir=`dirname "$0"`"/.."
dir=`realpath "$dir"`
name=`basename "$test" .json | sed 's/^[a-z][a-z]\-//' | sed 's/[0-9]*$//'`

echo "Target word: $name"

tre=`mktemp /tmp/tre.XXXXXX`
echo "$name" | $dir/tools/loadkb.py "$tre"

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$dir/curve/build"
#tmp=`mktemp /tmp/$name.XXXXXX.json`
#html=`mktemp /tmp/curvekb.$name.XXXXXX.html`

$dir/cli/build/cli -d "$tre" "$test" 2>&1 | grep -vi '^Result:'
# tee $tmp | grep -i "^Result:" | tail -n 1 | sed 's/^Result:\ *//' | $dir/tools/jsonresult2html.py > $html || exit 1

rm -f "$tre"
