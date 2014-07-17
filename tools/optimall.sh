#! /bin/sh -e

cd `dirname "$0"`
(
    for sc in max max2 stddev ; do
	echo "./optim.py $sc > /tmp/okboard-optim-$sc.log"
    done 
) | parallel

notify-send -t 3600000 "OKB optim done" "`date`"

echo "OK"
