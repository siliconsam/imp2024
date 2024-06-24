#!/bin/sh
# Simple IMP compiler shell wrapper

echo "$0"
echo "${PWD}"

IMP_SOURCE_HOME=$(PWD)
IMP_INSTALL_HOME=${IMP_SOURCE_HOME}/release

export IMP_SOURCE_HOME=${PWD}
export IMP_INSTALL_HOME=${IMP_SOURCE_HOME}/release

echo "${IMP_SOURCE_HOME}"
echo "${IMP_INSTALL_HOME}"

if [ ! -e ${IMP_INSTALL_HOME} ]; then
    mkdir ${IMP_INSTALL_HOME}
fi

if [ ! -e ${IMP_INSTALL_HOME}/bin ]; then
    mkdir ${IMP_INSTALL_HOME}/bin
fi
if [ ! -e ${IMP_INSTALL_HOME}/include ]; then
    mkdir ${IMP_INSTALL_HOME}/include
fi
if [ ! -e ${IMP_INSTALL_HOME}/lib ]; then
    mkdir ${IMP_INSTALL_HOME}/lib
fi

cd ${IMP_SOURCE_HOME}/pass3
make bootstrap
make install

cd ${IMP_SOURCE_HOME}/lib
make loadlinux
make bootstrap
make install

cd ${IMP_SOURCE_HOME}/compiler
make bootstrap
make install

cd ${IMP_SOURCE_HOME}/pass3
make clean

cd ${IMP_SOURCE_HOME}/lib
make clean

cd ${IMP_SOURCE_HOME}/compiler
make clean

cd ${IMP_SOURCE_HOME}
