#! /bin/bash

export MODULE=$1
export IMP_SLIMIBJ=$2
rm $MODULE.ibj
rm $MODULE.$2.*

imp77 -c -Fc -Fs -Fi -Fh slimibj.imp
imp77 -c -Fc -Fs -Fi -Fh boolean.imp
imp77 -c -Fc -Fs -Fi -Fh symbols.imp
imp77 -c -Fc -Fs -Fi -Fh externals.imp
imp77 -c -Fc -Fs -Fi -Fh locals.imp
imp77 -c -Fc -Fs -Fi -Fh labels.imp
imp77 -c -Fc -Fs -Fi -Fh ibjconversion.imp

NEWRUN=$2.00
cp $MODULE.ibj $MODULE.$NEWRUN.ibj
ibj2assemble $MODULE.$NEWRUN.ibj $MODULE.$NEWRUN.assemble
pass3elf $MODULE.$NEWRUN.ibj
cp $MODULE.$NEWRUN.o $MODULE.o
gcc -m32 -no-pie -o slimibj slimibj.o symbols.o labels.o ibjconversion.o /usr/local/lib/libimp77.a -lm -lc

OLDRUN=$2.00
NEWRUN=$2.01
./slimibj $MODULE.$OLDRUN.ibj $MODULE.$NEWRUN.ibj $MODULE.$NEWRUN.pre.lst $MODULE.$NEWRUN.post.lst
ibj2assemble $MODULE.$NEWRUN.ibj $MODULE.$NEWRUN.assemble
pass3elf $MODULE.$NEWRUN.ibj
cp $MODULE.$NEWRUN.o $MODULE.o
gcc -m32 -no-pie -o slimibj slimibj.o symbols.o labels.o ibjconversion.o /usr/local/lib/libimp77.a -lm -lc

OLDRUN=$2.01
NEWRUN=$2.02
./slimibj $MODULE.$OLDRUN.ibj $MODULE.$NEWRUN.ibj $MODULE.$NEWRUN.pre.lst $MODULE.$NEWRUN.post.lst
ibj2assemble $MODULE.$NEWRUN.ibj $MODULE.$NEWRUN.assemble
pass3elf $MODULE.$NEWRUN.ibj
cp $MODULE.$NEWRUN.o $MODULE.o
gcc -m32 -no-pie -o slimibj slimibj.o symbols.o labels.o ibjconversion.o /usr/local/lib/libimp77.a -lm -lc

OLDRUN=$2.02
NEWRUN=$2.03
./slimibj $MODULE.$OLDRUN.ibj $MODULE.$NEWRUN.ibj $MODULE.$NEWRUN.pre.lst $MODULE.$NEWRUN.post.lst
ibj2assemble $MODULE.$NEWRUN.ibj $MODULE.$NEWRUN.assemble
pass3elf $MODULE.$NEWRUN.ibj
cp $MODULE.$NEWRUN.o $MODULE.o
gcc -m32 -no-pie -o slimibj slimibj.o symbols.o labels.o ibjconversion.o /usr/local/lib/libimp77.a -lm -lc
