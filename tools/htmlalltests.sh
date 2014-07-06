#! /bin/sh -e

dir=`dirname "$0"`"/.."
cd "$dir" 
for t in test/* ; do echo tools/html.sh $t ; done | parallel
