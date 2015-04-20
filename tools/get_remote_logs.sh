#! /bin/bash -e

[ -f "$HOME/.okboard-test" ] && . $HOME/.okboard-test

host="$1"
becane_id="$2"

d=`dirname "$0"`
[ -n "$host" ] || host=${OKBOARD_REMOTE_HOST:-jolla} # <- my jolla hostname/ip address

log_dir=${OKBOARD_LOG_DIR:-/tmp/okboard-logs}
[ -d "$log_dir" ] || mkdir "$log_dir"

tmpbase="${OKBOARD_TMP_DIR:-/tmp}/download"
mkdir -p "$tmpbase"

for dir in $OKBOARD_TEST_DIR .local/share/okboard ; do
    if ssh "nemo@${host}" "test -d $dir" ; then
	id="${becane_id:-$host}"
	tmpdir="$tmpbase/${id}_"`echo "$dir" | tr -cs 'a-zA-Z_\-0-9.' '_' | sed 's/_*$//'`
	mkdir -p "$tmpdir"
	echo "==> Getting logs from $host:$dir --> $log_dir ($tmpdir) ..."
	rsync -av --progress --delete "nemo@$host:$dir/" "$tmpdir/"
	find "$tmpdir/" -name '*.log*' | sort | xargs -r cat | $d/parselog.py "$log_dir" "$id"  # sort for processing "curve*" before "predict*"
    fi
done

find "$tmpbase/" -type f -mtime +20 -delete
