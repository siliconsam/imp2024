.RECIPEPREFIX = >
CC = gcc ${M32}
CCFLAGS = -O

# LOOK! Change these three if they don't match your local policies
BASEDIR = ${IMP_INSTALL_HOME}
BINDIR = ${BASEDIR}/bin
LIBDIR = ${BASEDIR}/lib
INCDIR = ${BASEDIR}/include

OBJS=impcore-arrayutils.o \
     impcore-types.o \
     impcore-mathutils.o \
     impcore-signal.o \
     impcore-strutils.o \
     implib-arg.o \
     implib-debug.o \
     implib-env.o \
     implib-heap.o \
     implib-read.o \
     implib-strings.o \
     implib-trig.o \
     imprtl-check.o \
     imprtl-event.o \
     imprtl-io.o \
     imprtl-main.o \
     imprtl-file.o \
     imprtl-trap.o \
     imprtl-line.o \
     imprtl-limit.o \
     prim-rtl-file.o

IBJS=impcore-arrayutils.ibj \
     impcore-types.ibj \
     impcore-mathutils.ibj \
     impcore-signal.ibj \
     impcore-strutils.ibj \
     implib-arg.ibj \
     implib-debug.ibj \
     implib-env.ibj \
     implib-heap.ibj \
     implib-read.ibj \
     implib-strings.ibj \
     implib-trig.ibj \
     imprtl-check.ibj \
     imprtl-event.ibj \
     imprtl-io.ibj \
     imprtl-file.ibj \
     imprtl-main.ibj \
     imprtl-trap.ibj \
     imprtl-line.ibj \
     imprtl-limit.ibj

all:	libimp77.a

libimp77.a: $(OBJS)
> ar -r libimp77.a $(OBJS)
> ranlib libimp77.a

libimp77.so: $(OBJS)
> ${CC} -shared -fPIC -Wl,-soname,libimp77.so -o libimp77.so $(OBJS)

bootstrap: libimp77.so libimp77.a stdperm.imp
# First, install the libraries and core include file
> install -t $(LIBDIR) libimp77.a
> install -t $(LIBDIR) libimp77.so
> install -t $(INCDIR) stdperm.imp
# finally, ensure all text files have Unix line-endings
> dos2unix *.imp
> dos2unix *.c

install: libimp77.so libimp77.a stdperm.imp
# Install the libraries and core include file
> install -t $(LIBDIR) libimp77.a
> install -t $(LIBDIR) libimp77.so
> install -t $(INCDIR) stdperm.imp

clean: #
> rm -f libimp77.a
> rm -f libimp77.so
> rm -f *.o

superclean: #
> rm -f *.a
> rm -f *.so
> rm -f *.so.*
> rm -f *.o
> rm -f *.cod
> rm -f *.ibj
> rm -f *.icd
> rm -f *.lst
> rm -f *.obj

%.o: %.ibj
> ${BINDIR}/pass3elf $<

%.o: %.imp
# Ensure all IMP source files have Unix line-endings
> ${BINDIR}/imp77 -c -Fc -Fs -Fi $<

%.o: %.c
# Ensure all C source files have Unix line-endings
> $(CC) -c $(CCFLAGS) $<

loadlinux: #
> cp ./linux/*.ibj ./*
> cp ./linux/*.imp ./*
> cp ./linux/*.inc ./*

loadwindows: #
> cp ./windows/*.ibj ./*
> cp ./windows/*.imp ./*
> cp ./windows/*.inc ./*

storelinux: #
> cp ./implib-heap.ibj ./linux/*
> cp ./implib-heap.inc ./linux/*
> cp ./implib-trig.ibj ./linux/*
> cp ./implib-trig.inc ./linux/*
> cp ./imprtl-file.ibj ./linux/*
> cp ./imprtl-file.inc ./linux/*
> cp ./imprtl-main.ibj ./linux/*
> cp ./imprtl-main.imp ./linux/*

storewindows: #
> cp ./implib-heap.ibj ./windows/*
> cp ./implib-trig.ibj ./windows/*
> cp ./imprtl-file.ibj ./windows/*
> cp ./imprtl-main.ibj ./windows/*
> cp ./imprtl-main.imp ./windows/*
> cp ./implib-heap.inc ./windows/*
> cp ./implib-trig.inc ./windows/*
> cp ./imprtl-file.inc ./windows/*
