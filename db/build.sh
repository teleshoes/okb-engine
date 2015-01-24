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

cd `dirname "$0"`
mydir=`pwd`

cp -vauf $mydir/lang-*.cf $WORK_DIR/

cd $WORK_DIR
for target in depend all ; do
    make -j -f $mydir/makefile CORPUS_DIR=${CORPUS_DIR} TOOLS_DIR=$mydir/../tools $target
done

cp -vauf *.tre *.db $mydir/
