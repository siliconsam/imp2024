#!/bin/sh
# Simple IMP compiler shell wrapper

echo "$0"
echo "${PWD}"

if [ -z ${IMP_SOURCE_HOME} ]; then
    export IMP_SOURCE_HOME="${PWD}"
fi

if [ -z ${IMP_INSTALL_HOME} ]; then
    if [ ! -e ${PWD}/release ]; then
        mkdir ${PWD}/release
    fi
    if [ ! -e ${PWD}/release/bin ]; then
        mkdir ${PWD}/release/bin
    fi
    if [ ! -e ${PWD}/release/include ]; then
        mkdir ${PWD}/release/include
    fi
    if [ ! -e ${PWD}/release/lib ]; then
        mkdir ${PWD}/release/lib
    fi
    export "IMP_INSTALL_HOME=${PWD}/release"
fi

#if [ -z ${IMP_EXPORT_HOME} ]; then
#    
#fi

if [ -z ${IMP_TOOLS_HOME} ]; then
    export IMP_TOOLS_HOME="${IMP_INSTALL_HOME}"
fi

bash&
