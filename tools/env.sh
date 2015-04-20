#! /bin/sh
# environment setup

_me=${BASH_SOURCE[0]}
OKB_QML="../okboard/qml"

if [ -z "$_me" ] ; then
    echo "only bash is supported!"
else 
    _mydir=$(cd $(dirname "$_me") ; cd .. ; pwd)
    
    machine=`uname -m`
    ngram_lib=`find "${_mydir}/ngrams/build/" -type d -name "lib.*" | grep "$machine"`
    
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${_mydir}/curve/build"
    export QML2_IMPORT_PATH=`readlink -f "${_mydir}/${OKB_QML}"`
    export PATH="${PATH}:${_mydir}/cli/build:${_mydir}/tools"
    export PYTHONPATH="${ngram_lib}"
    OKBOARD_TEST_DIR=1
    [ -f "$HOME/.okboard-test" ] && . $HOME/.okboard-test
    export OKBOARD_TEST_DIR

fi

