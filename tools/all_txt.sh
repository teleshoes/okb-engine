#! /bin/bash -e

cd `dirname "$0"`/..

for t in test/*.json ; do
    lang="en"
    echo "$t" | grep '/fr-' >/dev/null && lang="fr"
    name=`basename "$t" .json`
    cli/build/cli -d "$@" db/fr.tre "$t" 2>/dev/null | tools/json2txt.py | sed 's/^/'"$name"' /'
done

