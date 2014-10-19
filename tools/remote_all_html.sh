#! /bin/sh -e

[ -f "$HOME/.okboard-test" ] && . $HOME/.okboard-test

d=`dirname "$0"`
host=${OKBOARD_REMOTE_HOST:-jolla} # <- my jolla hostname/ip address
out=${OKBOARD_HTML_DIR}
[ -z "$out" ] && out=`mktemp -d /tmp/okboard-remote-all.XXXXXX`
L="${OKBOARD_TEST_DIR:-/tmp}"

mkdir -p "$out"
rm -f "$out/index.html"
echo "Output dir: $out"
[ -n "$1" ] || rsync -av --progress --include "*.log*" "nemo@$host:$L/" "$out/"

cat $out/*.log* | $d/parselog.py $out && [ -f "$out/index.html" ] && firefox "$out/index.html"

