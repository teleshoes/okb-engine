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
make -j"$CPU"

pushd ngrams
python3 setup-fslm.py build
python3 setup-cdb.py build
popd

pushd cluster
create_makefile "cluster"
make -j"$CPU"
popd
