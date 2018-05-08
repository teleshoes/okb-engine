#! /bin/bash -e
# test to reproduce the dreadful tree corruption bug when learning new words
#
# it just creates a fake tree word database, then learns new words
# and then checks thats database contains all expected words

cd "$(dirname "$0")/.."

. tools/env.sh

tmp="$(mktemp -d "/tmp/$(basename "$0" .sh).XXXXXX")"
echo "Work directory: $tmp"
cd "$tmp"

# --- setup ---

INIT="plop toto bidon a b c"  # original language file
ADD="pipo plopi plopa totototo tôtô"  # user learned words

# create tree DB
tre="db.tre"
echo "$INIT" | tr ' ' '\n' | loadkb.py "$tre"

# learn some new words
for word in $ADD ; do
    echo "Adding '$word'"
    cli -L "$tre" "$word"
done

# --- assert ---
echo "$INIT $ADD" | tr ' ' '\n' | sort | uniq | grep -v '^$' > expected.txt
cli -D "$tre" | grep 'payload:' | sed 's/\ .*payload:\ */ /' | while read letters payload ; do echo "$payload" | sed "s/=/$letters/" | tr ',' '\n' ; done | tr -d '\1' | sort | uniq | grep -v '^$' > actual.txt

if ! cmp actual.txt expected.txt ; then
    diff -u expected.txt actual.txt || true
    echo "*Test failed*"
    exit 1
fi

echo "Success \o/"

rm -rf "$tmp" # only on success
