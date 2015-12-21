#! /bin/sh -xe

cd `dirname $0`
make -j9
cd ngrams
python3 setup-fslm.py build
python3 setup-cdb.py build
