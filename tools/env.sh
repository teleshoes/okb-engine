#! /bin/bash
# environment setup

_me=${BASH_SOURCE[0]}
OKB_QML="../okboard/qml"

_filter_path() {
    local parent="$1"
    local path="$2"
    echo "$path" | tr ':' '\n' | grep -v "$parent" | grep -v '^$' | tr '\n' ':'
}

if [ -z "$_me" ] ; then
    echo "only bash is supported!"
else 
    _mydir=$(cd $(dirname "$_me") ; cd .. ; pwd)

    _parent=$(dirname "$_mydir")
    
    machine=$(uname -m)
    ngram_lib=$(find "${_mydir}/ngrams/build/" -type d -name "lib.*" | grep '\b'"$machine"'\b' | sort | tail -n 1)
    
    export LD_LIBRARY_PATH=$(_filter_path "$_parent" "${LD_LIBRARY_PATH}")":${_mydir}/curve/build"
    export QML2_IMPORT_PATH=`readlink -f "${_mydir}/${OKB_QML}"`
    export PATH=$(_filter_path "$_parent" "${PATH}")":${_mydir}/cli/build:${_mydir}/tools"
    export PYTHONPATH="${ngram_lib}"
    OKBOARD_TEST_DIR=1
    [ -f "$HOME/.okboard-test" ] && . $HOME/.okboard-test
    export OKBOARD_TEST_DIR

fi

