#! /bin/bash -e
# torture test for new word learning

cd "$(dirname "$0")/.."

. tools/env.sh

dump() {
    local db="$1"
    cli -D "$db" | tr -d '\1' | grep -a 'payload:' | sed 's/\ .*payload:\ */ /' | awk '{ print gensub(/=/,$1,"g",$2) }' | tr ',' '\n' | sort | uniq | ( grep -av '^$' || true)
}

tmp="$(mktemp -d "/tmp/$(basename "$0" .sh).XXXXXX")"
echo "Work directory: $tmp"
cp db/en.tre $tmp/db.tre
dump db/fr.tre > $tmp/words.txt

cd "$tmp"

DB="db.tre"
userf="${DB%.tre}-user.txt"
rm -f "$userf"
touch "$userf"
dump "$DB" > actual.txt
cp actual.txt expected.txt

cat words.txt | shuf | (
    c=0
    while read word ; do
	len="$(echo -n "$word" | wc -c | awk '{ print $1 }')"
	if [ "$len" -lt 2 ] ; then continue ; fi

	c="$(expr "$c" + 1)"
	l="$(wc -l "$userf" | awk '{ print $1 }')"
	echo "[#$c : $l] ========= word: [$word] len=$len"

	cli -L "$DB" "$word" >/dev/null 2>&1
	dump "$DB" > actual.txt 2>/dev/null
	( cat expected.txt ; echo "$word" ) | sort | uniq > expected.txt.tmp && mv -f expected.txt.tmp expected.txt

	if ! cmp actual.txt expected.txt ; then
	    diff -u actual.txt expected.txt || true
	    echo "*BOOM*"
	    exit 1
	fi

    done
)



