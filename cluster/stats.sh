#! /bin/sh -e

set -o pipefail

D="$1"
[ ! -d "$D" ] && echo "usage: $0 <corpus directory>" && exit 1
J=4

set -o pipefail

> stats.txt.tmp
for lang in fr en ; do
    if [ ! -f /tmp/in-$lang.bz2 ] ; then
	( lbzip2 -d < $D/clusters-$lang.txt.bz2; echo LEARN; lbzip2 -d < $D/gram-$lang-learn.csv.bz2; echo TEST; lbzip2 -d < $D/gram-$lang-test.csv.bz2 ) | bzip2 -9 > /tmp/in-$lang.bz2.tmp && mv -f /tmp/in-$lang.bz2.tmp /tmp/in-$lang.bz2
    fi

    for n in 2 3 ; do
	opt=
	[ "$n" = 2 ] && opt="-g"
	
	(
	    for c in 7 8 9 10 11 ; do
		for w in 50000 100000 200000 300000 350000 ; do
		    echo "lbzip2 -d < /tmp/in-$lang.bz2 | ./clustertest.pl $opt -c $c -C $w -W `expr 400000 - $w`"
		done
	    done
	) | parallel -j $J | tee result-$lang-$n.txt

	cat result-$lang-$n.txt | grep '^Result' | cut -c8- | sort -n | sed 's/^/'"$lang-$n"';/' >> stats.txt.tmp

    done

done


mv -vf stats.txt.tmp stats.txt
