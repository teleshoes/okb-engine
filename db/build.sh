#! /bin/sh -e
# build language files out of tree
#  (just a noob-friendly wrapper for the makefile)

cf="$HOME/.okboard-build"

[ -f "$cf" ] && . $cf

if [ -z "$WORK_DIR" ] ; then
    echo "Please set the following variables (environment or ~/.okboard-build file):"
    echo " - CORPUS_DIR: where to find text corpora (corpus-<lang>.txt.bz2)"
    echo " - WORK_DIR: where to put temporary files (defaults to /tmp)"
    exit 1
fi

die() { echo "DIE: $*"; exit 1; }

[ -n "$WORK_DIR" ] || WORK_DIR="/tmp"
[ -d "$WORK_DIR" ] || die "working directory not found: $WORK_DIR"
[ -d "$CORPUS_DIR" ] || die "corpus directory not found: $CORPUS_DIR"

add_tgt=
if [ -n "$1" ] ; then
    if [ "$1" = "-r" ] ; then add_tgt=clean ; else die "bad command line option: $1, only supported option is '-r' to force a full rebuild" ; fi
fi

cd `dirname "$0"`
mydir=`pwd`

. $mydir/../tools/env.sh

cp -vauf $mydir/lang-*.cf $mydir/db.version $WORK_DIR/

cd $WORK_DIR
for target in $add_tgt depend all ; do
    make -j -f $mydir/makefile CORPUS_DIR=${CORPUS_DIR} TOOLS_DIR=$mydir/../tools $target
done

cp -vauf *.tre *.db *.ng $mydir/
