#! /bin/sh -e

db="$1"
[ -z "$db" -o ! -f "$db" ] && echo "usage: "`basename "$0"`" <predict db file>" && exit 1

set -x
sqlite3 "$db" 'select w1.word, w2.word, w3.word, g.stock_count, g.user_count, g.user_replace, g.last_time from words w1, words w2, words w3, grams g where g.user_count > 0 and g.id1 = w1.id and g.id2 = w2.id and g.id3 = w3.id order by g.user_count'

