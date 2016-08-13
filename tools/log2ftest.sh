#! /bin/bash -e
# replay swipes that occured on device but with current code (curve matching & predict engine)
# (you'll have to fetch & process them regularly with get_remote_logs.sh script)
# This allow to cheaply produce a huge amount of non regression tests and an overall quality metric
#
# Expected results should be updated by hand in produced org-mode file (or unchecked if they are bad tests)
# Content of this file is retained between uses

set -o pipefail
cd `dirname "$0"`
cd ..

. tools/env.sh

FT_FILE="tools/ftest.org"

[ -f "$HOME/.okboard-test" ] && . $HOME/.okboard-test
log_dir=${OKBOARD_LOG_DIR:-/tmp/okboard-logs}
work_dir=${OKBOARD_FTEST_DIR:-/tmp/ftest}

mkdir -p "$work_dir"

max_age=
reset=
params=
opts=
force_check=
only_selected=
quick=

usage() {
    echo "usage :"`basename "$0"`" <opts>"
    echo "opts:"
    echo " -m <max_age>"
    echo " -z : reset"
    echo " -p <parameters> : parameters for prediction engine"
    echo " -f : force check"
    echo " -s : only process selected strokes"
    echo " -q : quick (do not generate json files & reports, disable debug log)"
    echo " -a : add new strokes found in logs"
}

while [ -n "$1" ] ; do
    case "$1" in
	-m) max_age="$2" ; shift ;;
	-z) reset=1 ;;
	-p) params="$2" ; shift ;;
	-a) opts="${opts}-a " ;;
	-f) force_check=1 ;;
	-s) only_selected=1 ;;
	-q) quick=1 ; opts="${opts}-q " ; only_selected=1 ;;
	-h) usage ; exit 0 ;;
	*) echo "Unknown parameter: $1" ; usage ; exit 1 ;;
    esac
    shift
done

[ -n "$CLI_OPTS" ] || export CLI_OPTS="-a 1"

getlogs() {
    local filt=
    [ -n "$max_age" ] && filt="-mtime -${max_age}"
    find "$log_dir/" -name '2*.log.bz2' $filt | grep -v '/\.' | xargs -r lbzip2 -dc
}

ts2="$work_dir/.ts2"
last=`ls -rt "$log_dir"/*/2*.log.bz2 | tail -n 1`
[ -n "$last" ] && [ -f "$ts2" ] && [ "$ts2" -ot "$last" ] && force_check=1 && echo "force_check=1"

ts="$work_dir/.ts"
rescan=
[ -z "$force_check" -a -z "$reset" -a -f "$ts" ] || rescan=1

selected=
[ -f "$FT_FILE" ] && selected="$(egrep ' \[X\] ' $FT_FILE | awk '{ print $3 }')"

if [ -n "$rescan" ] ; then
    getlogs | egrep '^(==WORD==|OUT:)' | (
	n=0

	tmpjson=`mktemp /tmp/log2ftest.XXXXXX`
	while read prefix id word suffix ; do
	    read -r prefix js  # -r does not interpret backslash as an escape character
	    [ "$prefix" = "OUT:" ]

	    n=`expr "$n" + 1`
	    word=`echo "$word" | sed -r "s/^'(.*)'$/\1/"`

	    pre="$work_dir/$id"

	    if [ -n "$only_selected" ] && [ -n "$selected" ] && ! echo "$selected" | grep '^'"$id"'$' >/dev/null ; then
		echo "Skipping $id (not selected)" >&2
		continue
	    fi

	    if [ -n "$quick" ] ; then
		# quick mode when we need the result quickly
		if [ ! -f "$pre.txt" -o -n "$reset" ] ; then
		    lang=$(echo "$js" | head -n 1 | sed 's/^.*treefile[^\}]*\///' | less | cut -c 1-2)

		    in="$pre.in.json"
		    echo "$js" > $in
		    quote_word=$(tools/quoteshell.py "$word")
		    cmd="tools/json-recover-keys.py \"$tmpjson\" < $in | "
		    cmd="${cmd} cli/build/cli ${CLI_OPTS} -e ${quote_word} -g -s -d db/${lang}.tre > $pre.txt 2> $pre.err"
		    echo "[$n] $id $word ($lang) --> $cmd" >&2
		    echo "$cmd"
		fi

	    else
		# traditional mode with all fancy reporting & logging
		if [ ! -f "$pre.png" -o -n "$reset" ] ; then
		    js=$(echo "$js" | tools/json-recover-keys.py "$tmpjson")

		    lang=$(echo "$js" | head -n 1 | sed 's/^.*treefile[^\}]*\///' | less | cut -c 1-2)

		    in="$pre.in.json"
		    log="$pre.log"
		    json="$pre.json"
		    html="$pre.html"
		    png="$pre.png"

		    echo "$js" > $in
		    cmd="cli/build/cli ${CLI_OPTS} -e \"${word}\" -d db/${lang}.tre \"$in\" > $log 2>&1 && "
		    cmd="${cmd} cat $log | grep -i '^Result:' | tail -n 1 | sed 's/^Result:\ *//' > $json && "
		    cmd="${cmd} cat $json | tools/jsonresult2html.py \"$word\" > $html.tmp && mv -f $html.tmp $html && "
		    cmd="${cmd} cat $json | tools/jsonresult2html.py --image \"$png\""

		    rm -f "$work_dir/$id.wt.log"

		    echo "[$n] $id $word ($lang) --> $cmd" >&2
		    echo "$cmd"
		fi
	    fi

	done
	touch "$ts"
	rm -f "$tmpjson"
    ) | parallel

fi

getlogs | tools/ftest_org.py $opts "tools/ftest.org" "$work_dir" $params

if [ -n "$rescan" ] && [ -z "$quick" ] ; then
    cat "$work_dir/manifest.txt" | cut -d';' -f 1-2 | tr ';' ' ' | (
	while read id exp ; do
	    if [ ! -f "$work_dir/$id.wt.log" ] ; then
		cmd="tools/word_test.sh $work_dir/$id.json \"$exp\" > $work_dir/$id.wt.tmp && mv -f $work_dir/$id.wt.tmp $work_dir/$id.wt.log"
		echo "$cmd"
	    fi
	done
    ) | parallel
fi

touch "$ts2"
