#! /bin/bash -e
# build language files out of tree
#  (just a noob-friendly wrapper for the makefile)

set -o pipefail

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

usage() {
    echo "usage: "`basename "$0"`" [-r] [<language codes 1> [ ... <langage code N> ] ]"
    exit 1
}

add_tgt=
langs=
while [ -n "$1" ] ; do
    case "$1" in
	-r) add_tgt=clean ;;
	-*) usage ;;
	*) langs="$langs $1" ;;
    esac
    shift
done

if [ -n "$langs" ] ; then
    echo "Building for languages: $langs"
    tgt_list="$add_tgt"
    for lang in $langs ; do
	tgt_list="${tgt_list} .depend-${lang} all-${lang}" # build a specific language
    done
else
    echo "Building for all languages"
    tgt_list="$add_tgt depend all" # build all languages
fi

cd `dirname "$0"`
mydir=`pwd`

# build python modules
pushd ../ngrams
python3 setup-cdb.py build
python3 setup-fslm.py build
popd

# build clustering tool
pushd ../cluster
if [ ! -f "Makefile" ] ; then
    qmake=${QMAKE:-qmake}
    $qmake -query QT_VERSION | grep '^5.' >/dev/null || die "QT5 + qmake is required"
    mkdir -p build
    $qmake
fi
make
popd

. $mydir/../tools/env.sh

cp -vauf $mydir/lang-*.cf $mydir/add-words-*.txt $mydir/db.version $WORK_DIR/

cd $WORK_DIR
for target in $tgt_list ; do
    make -j -f $mydir/makefile CORPUS_DIR=${CORPUS_DIR} TOOLS_DIR=$mydir/../tools $target
done

rsync -av *.tre *.db *.ng $mydir/
