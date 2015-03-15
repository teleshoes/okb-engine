#! /bin/sh 


tmp=`mktemp -d /tmp/okboard-compareall.XXXXXX`

cd `dirname "$0"`"/.."
fail=
for test in test/*.json ; do
    name=`basename $test .json`
    echo "=== $name ==="
    kb="en"
    echo $name | grep '^fr-'>/dev/null && kb="fr"
    for a in 0 1 2 ; do
	./cli/build/cli -g -s -a $a -d -s "db/$kb.tre" "$test" > $tmp/$a 2>/dev/null
    done

    for a in 1 2 ; do
	if ! tools/compare1.py $tmp/0 $tmp/$a ; then
	    # diff -u $tmp/0 $tmp/$a
	    echo "Fail: $test"
	    fail="$fail$name:$a "
	fi
    done

done

[ -n "$fail" ] && echo "Fail list: $fail" && exit 1
