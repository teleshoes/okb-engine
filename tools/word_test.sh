#! /bin/bash -e
# just check curve matching plugin with expected word
# (generates far less output)

test="$1"
word="$2"
if [ ! -f "$test" ] ; then echo "usage: $0 <json test file> [<word to test>]" ; exit 1; fi
dir=`dirname "$0"`"/.."
dir=`readlink -f "$dir"`
lang_re=$(ls "$dir/db/" | grep '^..\.tre' | cut -c1,2 | tr '\n' '|' | sed 's/|$//')
[ -n "$word" ] || word=`basename "$test" .json | sed -r 's/^('"${lang_re}"')\-//' | sed 's/[0-9]*$//' | sed 's/\-.*$//'`

echo "Target word: $word"

tre=`mktemp /tmp/tre.XXXXXX`
echo "$word" | $dir/tools/loadkb.py "$tre"

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$dir/curve/build"

$dir/cli/build/cli ${CLI_OPTS} -d "$tre" "$test" 2>&1 | grep -vi '^Result:'

rm -f "$tre"
