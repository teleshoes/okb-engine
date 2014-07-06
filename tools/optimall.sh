#! /bin/sh -e

cd `dirname "$0"`
(
    for sc in max max2 stddev ; do
	echo "./optim.py $sc"
    done 
) | parallel

notify-send -t 3600000 "Optim done" "`date`"

echo "OK"
