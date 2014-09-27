#! /bin/sh -e

dir=`dirname "$0"`"/.."
dir=`realpath "$dir"`
cd "$dir"

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$dir/curve/build"

(
    for test in test*/*.json ; do
	name=`basename "$test" .json`
	
	lang="en"
	if echo "$name" | egrep '^[a-z][a-z]\-' >/dev/null ; then
	    lang=`echo "$name" | head -n 1 | cut -c 1,2`
	fi
	
	echo "echo \"=== $test [$name] ===\" ; cli/build/cli -d db/$lang.tre $test 2>&1 | grep '^SORT>' ; echo"
    done
) | parallel

 
