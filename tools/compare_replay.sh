#! /bin/bash -e

r1="$1"
r2="$2"
cmp="$3"

if [ -z "$cmp" ] ; then
    echo "usage: "$(basename "$0")" <replay output 1> <replay output 2> <compare read/write file>"
    exit 1
fi

set -o pipefail

t1=$(mktemp /tmp/compare_replay.XXXXXX)
t2=$(mktemp /tmp/compare_replay.XXXXXX)

trap "rm -f $t1 $t2" EXIT

cat "$r1" | grep '^\[\*\] Guess' | cut -c 11- | sed 's/\ .*\ / /' | sort -n > $t1
cat "$r2" | grep '^\[\*\] Guess' | cut -c 11- | sed 's/\ .*\ / /' | sort -n > $t2

date=$(date)
(
    echo "# [$date] replay diff $r1 -> $r2"
    echo
    echo "* differences list"
    paste "$t1" "$t2" | awk '{ print $1 " " $2 " " $4 }' |
	while read id st1 st2 ; do
	    [ "$st1" = "$st2" ] && continue

	    l1=$(grep -n "^ID: ${id} " "$r1" | cut -d: -f 1)
	    l2=$(grep -n "^ID: ${id} " "$r2" | cut -d: -f 1)

	    comment=
	    [ -f "$cmp" ] && comment="$(cat "$cmp" | grep " $id " | sed -r 's/^.*\]\]\ *//')"

	    echo "  - [ ] $id [[file:$r1::$l1][$st1]] -> [[file:$r2::$l2][$st2]] $comment"
	done
) | tee "$cmp.tmp" && mv -f "$cmp.tmp" "$cmp"
