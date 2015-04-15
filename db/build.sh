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

usage() {
    echo "usage: "`basename "$0"`" [-r] [<language code>]"
    exit 1
}

add_tgt=
lang=
if [ -n "$1" ] ; then
    case "$1" in
	-r) add_tgt=clean ; shift ;;
	-*) usage ;;
	*) lang="$1" ; shift ;;
    esac
    [ -n "$1" ] && [ -z "$lang" ] && lang="$1"
fi

if [ -n "$lang" ] ; then
    echo "Building for language: $lang"
    tgt_list="$add_tgt .depend-${lang} all-${lang}" # build a specific language
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

. $mydir/../tools/env.sh

cp -vauf $mydir/lang-*.cf $mydir/db.version $WORK_DIR/

cd $WORK_DIR
for target in $tgt_list ; do
    make -j -f $mydir/makefile CORPUS_DIR=${CORPUS_DIR} TOOLS_DIR=$mydir/../tools $target
done

rsync -av *.tre *.db *.ng $mydir/
