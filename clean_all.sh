#! /bin/bash -xe

CPU=$(getconf _NPROCESSORS_ONLN)
cd $(dirname $0)

create_makefile() {
    local project="$1"
    if [ ! -f "Makefile" ] || [ "Makefile" -ot "$project.pro" ] ; then
	qtchooser --run-tool=qmake -qt=5
    fi
}

create_makefile "okboard"
make clean

pushd ngrams
python3 setup-fslm.py clean
python3 setup-cdb.py clean
popd

pushd cluster
make clean
popd
