#! /bin/bash -xe

cd $(dirname $0)
if [ ! -f "Makefile" ] || [ "Makefile" -ot "okboard.pro" ] ; then
    qtchooser --run-tool=qmake -qt=5
fi
CPU=$(getconf _NPROCESSORS_ONLN)
make -j"$CPU"
cd ngrams
python3 setup-fslm.py build
python3 setup-cdb.py build
