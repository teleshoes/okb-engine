#! /bin/sh -xe
# just check curve matching plugin with expected word
# (generates far less output)

test="$1"
word="$2"
if [ ! -f "$test" ] ; then echo "usage: $0 <json test file> [<word to test>]" ; exit 1; fi
dir=`dirname "$0"`"/.."
dir=`realpath "$dir"`
[ -n "$word" ] || word=`basename "$test" .json | sed 's/^[a-z][a-z]\-//' | sed 's/[0-9]*$//'`

echo "Target word: $word"

tre=`mktemp /tmp/tre.XXXXXX`
echo "$word" | $dir/tools/loadkb.py "$tre"

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$dir/curve/build"

$dir/cli/build/cli -d "$tre" "$test" 2>&1 | grep -vi '^Result:'

rm -f "$tre"
