#! /bin/bash

lang=
expected=
tre=
while [ -n "$1" ] ; do
    case "$1" in
	-l) lang="$2" ; shift ;;
	-e) expected="$2" ; shift ;;
	-t) tre="$2" ; shift ;;
	-*) echo "Unknown option: $1" ; exit 1 ;;
	*) break ;;
    esac
    shift
done

test="$1"
edit="$2"
if [ ! -f "$test" ] ; then echo "usage: "$(basename "$0")" [-t <tre db file>] [-l <lang>] [-e <expected word>] <json test file> [<editor for log file>]" ; exit 1; fi
dir=`dirname "$0"`"/.."
dir=`readlink -f "$dir"`
name=`basename "$test" .json`

if [ -n "$tre" ] ; then
    lang="(unspecified)"
    name="$(echo "$name" | sed 's/^[a-z][a-z]\-//')"

else
    lang_re=$(ls "$dir/db/" | grep '^..\.tre' | cut -c1,2 | tr '\n' '|' | sed 's/|$//')
    if echo "$name" | egrep '^('"${lang_re}"')\-' >/dev/null ; then
	lang=$(echo "$name" | head -n 1 | cut -c 1,2)
	name=$(echo "$name" | cut -c4-)
    elif [ -z "$lang" ] ; then
	lang="en"
    fi
    name=$(echo "$name" | sed 's/\-.*//' | sed 's/[0-9]*$//')

    tre="$dir/db/${lang}.tre"
fi

[ -n "$expected" ] && name="$expected"

echo "Lang: $lang, expected word: $name, database: $tre"

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$dir/curve/build"
tmp=`mktemp /tmp/$name.XXXXXX.json`
html=`mktemp /tmp/curvekb.$name.XXXXXX.html`
if $dir/cli/build/cli ${CLI_OPTS} -d "$tre" "$test" 2>&1 | tee $tmp | grep -i "^Result:" | tail -n 1 | sed 's/^Result:\ *//' | $dir/tools/jsonresult2html.py "$name" > $html; then
    if [ -n "$edit" ] ; then
	edit=`which "$edit" 2>/dev/null`
	[ -n "$edit" ] && $edit $tmp
    fi
    xdg-open "$html"
else
    echo "FAILED!"
fi
echo "Log file: file://$tmp"
