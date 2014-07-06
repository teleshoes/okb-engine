#! /bin/sh
# environment setup

_me=${BASH_SOURCE[0]}
OKB_QML="../okb-keyboard/qml"

if [ -z "$_me" ] ; then
    echo "only bash is supported!"
else 
    _mydir=$(cd $(dirname "$_me") ; cd .. ; pwd)
    
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${_mydir}/curve/build"
    export QML2_IMPORT_PATH=`realpath "${_mydir}/${OKB_QML}"`
    export PATH="${PATH}:${_mydir}/cli/build:${_mydir}/tools"
    export OKBOARD_TEST=1

fi

