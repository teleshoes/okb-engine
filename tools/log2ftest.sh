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

[ -f "$HOME/.okboard-test" ] && . $HOME/.okboard-test
log_dir=${OKBOARD_LOG_DIR:-/tmp/okboard-logs}
work_dir=${OKBOARD_FTEST_DIR:-/tmp/ftest}

mkdir -p "$work_dir"

max_age=
reset=

while [ -n "$1" ] ; do
    case "$1" in
	-a) max_age="$2" ; shift ;;
	-z) reset=1 ;;
	*) echo "Unknown parameter: $1" ; exit 1 ;;
    esac
    shift
done
TEST="$1"

getlogs() {
    local filt=
    [ -n "$max_age" ] && filt="-mtime -${max_age}"
    find "$log_dir/" -name '2*.log.bz2' $filt | grep -v '/\.' | xargs -r lbzip2 -dc
}

bin="curve/build/libcurveplugin.so"
ts="$work_dir/.ts"
if [ -z "$reset" -a -f "$ts" -a "$bin" -ot "$ts" ] ; then
    : # no change
else
    
    getlogs | egrep '^(==WORD==|OUT:)' | (
	n=0
	manifest="$work_dir/manifest.txt"
	> $manifest

	while read prefix id word suffix ; do
	    read prefix js
	    [ "$prefix" = "OUT:" ]
	    
	    n=`expr "$n" + 1`
	    word=`echo "$word" | sed -r "s/^'(.*)'$/\1/"`
	    	    
	    pre="$work_dir/$id"

	    echo "$id $word" >> $manifest
	    
	    if [ ! -f "$pre.png" -o "$pre.png" -ot "$bin" -o -n "$reset" ] ; then
		
		lang=`echo "$js" | head -n 1 | sed 's/^.*treefile[^\}]*\///' | less | cut -c 1-2`
		
		in="$pre.in.json"
		log="$pre.log"
		json="$pre.json"
		html="$pre.html"
		png="$pre.png"
		
		echo "$js" > $in
		cmd="cli/build/cli -a 1 -d db/${lang}.tre \"$in\" > $log 2>&1 && "
		cmd="${cmd} cat $log | grep -i '^Result:' | tail -n 1 | sed 's/^Result:\ *//' > $json && "
		cmd="${cmd} cat $json | tools/jsonresult2html.py \"$word\" > $html.tmp && mv -f $html.tmp $html && "
		cmd="${cmd} cat $json | tools/jsonresult2html.py --image \"$png\""
		
		echo "[$n] $id $word ($lang) --> $cmd" >&2
		echo "$cmd"
	    fi

	done
	touch "$ts"
    ) | parallel

fi

getlogs | tools/ftest_org.py "tools/ftest.org" "$work_dir"
