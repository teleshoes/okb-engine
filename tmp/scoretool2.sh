#! /bin/bash -e

set -o pipefail

OUT=${1:-/tmp/scoretool2}
mkdir -p "$OUT"
OUT=`readlink -f "$OUT"`

cd `dirname "$0"`
mydir=`pwd`
cd ..

TS="$OUT/.ts"
[ "$TS" -ot "curve/build/libcurveplugin.so" ] && find "$OUT/" -type f -name "test*" -delete
touch "$TS"

for t in test*/*.json ; do
    id=`echo "$t" | tr '/' '.' | sed 's/\.json$//'`

    if [ ! -s "$OUT/$id.html" ] ; then
	name=`basename "$t" .json`
	lang="en"
	echo "$name" | egrep '^[a-z][a-z]\-' >/dev/null && lang=`echo "$name" | head -n 1 | cut -c 1,2`

	log="$OUT/$id.log"
	json="$OUT/$id.json"
	html="$OUT/$id.html"

	cmd="cli/build/cli ${CLI_OPTS} -d \"db/${lang}.tre\" \"$t\" > $log 2>&1 && "
	cmd="${cmd} cat $log | grep -i '^Result:' | tail -n 1 | sed 's/^Result:\ *//' > $json && "
	cmd="${cmd} cat $json | tools/jsonresult2html.py $name > $html.tmp && mv -f $html.tmp $html"

	echo "$t" >&2
	echo "$cmd"
    fi
done | parallel

cd $OUT
$mydir/scoretool2_html.py > index.html
firefox index.html
