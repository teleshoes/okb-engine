#! /bin/sh -e

db="$1"
[ -z "$db" -o ! -f "$db" ] && echo "usage: "`basename "$0"`" <predict db file>" && exit 1

set -x
sqlite3 "$db" 'update grams set user_count = 0, user_replace = 0, last_time = 0'
