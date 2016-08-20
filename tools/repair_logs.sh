#! /bin/bash -e
# one-shot repair of old log files

dir="$1"

[ ! -d "$dir" ] && echo "dir?" && exit 1

set -o pipefail
tmp=/tmp/persist.json # $(mktemp)

find "$dir/" -name "*.log.bz2" -type f | (
    while read file ; do
	echo "File $file"
	lbzip2 -d < "$file" | (
	    while read li ; do
		if echo "$li" | grep '^OUT:' >/dev/null ; then
		    echo -n 'OUT: '
		    echo "$li" | cut -c6- | sed -r 's/"([a-z]+)""/\[\1\]"/g' | $(dirname "$0")/json-recover-keys.py "$tmp"
		else
		    echo "$li"
		fi
	    done
	) | lbzip2 -9 > "$file.tmp" && mv -vf "$file.tmp" "$file"
    done
)

rm -f "$tmp"

echo "OK"
