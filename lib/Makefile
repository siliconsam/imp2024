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
     imprtl-file.o \
     imprtl-trap.o \
     imprtl-line.o \
     imprtl-limit.o \
     prim-rtl-file.o \
     imprtl-main.o

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

LIST=implib-heap.inc \
     implib-heap.ibj \
     imprtl-file.inc \
     imprtl-file.ibj \
     imprtl-main.imp \
     imprtl-main.ibj \
     implib-trig.inc \
     implib-trig.ibj

all:	libimp77.a
> @echo "Completed lib make ALL"

#bootstrap: libimp77.so libimp77.a stdperm.imp
bootstrap: libimp77.a stdperm.imp
# install the libraries and core include file
> @install -t $(LIBDIR) libimp77.a
> @#install -t $(LIBDIR) libimp77.so
> @install -t $(INCDIR) stdperm.imp
> @echo "Completed lib make BOOTSTRAP"

libimp77.a: $(OBJS)
> @ar -c -r libimp77.a $(OBJS)
> @ranlib libimp77.a
> @echo "Completed lib make LIBIMP77.A"

libimp77.so: $(OBJS)
> @${CC} -shared -fPIC -Wl,-soname,libimp77.so -o libimp77.so $(OBJS)
> @echo "Completed lib make LIBIMP77.SO"

rebuild: libimp77.so libimp77.a stdperm.imp
# First, install the libraries and core include file
# finally, ensure all text files have Unix line-endings
> @echo "Completed lib make REBUILD"

install: libimp77.so libimp77.a stdperm.imp
# Install the libraries and core include file
> @install -t $(LIBDIR) libimp77.a
> @#install -t $(LIBDIR) libimp77.so
> @install -t $(INCDIR) stdperm.imp
> @echo "Completed lib make INSTALL"

clean: #
> @rm -f *.a
> @rm -f *.so
> @rm -f *.o
> @rm -f *.cod
> @rm -f *.icd
> @rm -f *.lst
> @echo "Completed lib make CLEAN"

superclean: clean
> @rm -f *.ibj
> @echo "Completed lib make SUPERCLEAN"

%.o: %.ibj
#%.o: $(IBJS)
> @${BINDIR}/pass3elf $<

#%.o: %.imp
## Ensure all IMP source files have Unix line-endings
#> ${BINDIR}/imp77 -c -Fc -Fs -Fi $<

%.ibj: %.imp
# Ensure all IMP source files have Unix line-endings
> ${BINDIR}/imp77 -c -Fc -Fs -Fi $<

%.o: %.c
# Ensure all C source files have Unix line-endings
> @$(CC) -c $(CCFLAGS) $<

loadlinux: #
> @cp -f ./linux/* ./
> @echo "Completed lib make LOADLINUX"

loadwindows: #
> @cp -f ./windows/* ./
> @echo "Completed lib make LOADWINDOWS"

storelinux: $(LIST)
> @cp -f $(LIST) ./linux
> @echo "Completed lib make STORELINUX"

storewindows: $(LIST)
> @cp -f $(LIST) ./windows
> @echo "Completed lib make STOREWINDOWS"
