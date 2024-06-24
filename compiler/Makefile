.RECIPEPREFIX = >

CC = gcc -no-pie ${M32}
CCFLAGS = -O

BASEDIR = ${IMP_INSTALL_HOME}
BINDIR = ${BASEDIR}/bin
LIBDIR = ${BASEDIR}/lib
INCDIR = ${BASEDIR}/include

# Library explicitly located, rather than searched by linker
# So that we can use the newly created library (in ../lib)
# Otherwise use the "official" library in $(BINDIR)
# Also use the possibly "tweaked" local loader script
#LD_SCRIPT = ../pass3/ld.i77.script
#IMP_LIB = ../lib/libimp77.a
#LINK_OPT = ../lib/libimp77.a -lm -T $(LD_SCRIPT)
LD_SCRIPT = $(BINDIR)/ld.i77.script
IMP_LIB = $(LIBDIR)/libimp77.a
LINK_OPT = $(LIBDIR)/libimp77.a -lm -T $(LD_SCRIPT)
#LD_SCRIPT = $(BINDIR)/ld.i77.script
#IMP_LIB = $(LIBDIR)/libimp77.so
#LINK_OPT = -limp77 -lm -T $(LD_SCRIPT)

all: takeon pass1 pass2

# We need to build pass1,pass2 from their .o files (created by the cross build script make.bat)
# Also build pass3 completely from source
bootstrap: takeon.ibj pass1.ibj pass2.ibj ibj.utils.ibj icd.utils.ibj $(IMP_LIB) $(LD_SCRIPT)
#bootstrap: pass1 pass2 takeon $(IMPLIB) ld.i77.script
# Just in case convert source files to have Linux line-endings
> dos2unix i77.grammar
> dos2unix *.imp
> dos2unix *.ibj
# Now build the programs
# First create the .o files from the .obj files
> ${BINDIR}/pass3elf takeon.ibj
> ${BINDIR}/pass3elf ibj.utils.ibj
> ${BINDIR}/pass3elf icd.utils.ibj
> ${BINDIR}/pass3elf pass1.ibj
> ${BINDIR}/pass3elf pass2.ibj

> $(CC) -o takeon takeon.o                       $(LINK_OPT)
> $(CC) -o pass1 pass1.o icd.utils.o             $(LINK_OPT)
> $(CC) -o pass2 pass2.o icd.utils.o ibj.utils.o $(LINK_OPT)

# Lastly install the three programs
> install -t $(BINDIR) takeon
> install -t $(BINDIR) pass1
> install -t $(BINDIR) pass2

# We need to build pass1, pass2 and pass3
install: takeon pass1 pass2
# Firstly, install the parser generator
> install -t $(BINDIR) takeon
# Next, install the executables for pass1,pass2
> install -t $(BINDIR) pass1
> install -t $(BINDIR) pass2

# do a minimal tidy up of programs and temporary files
clean: #
> rm -f takeon
> rm -f pass1
> rm -f pass2
> rm -f i77.tables.inc
> rm -f *.o
> rm -f *.obj
> rm -f *.debug

# really scrub away all programs and temporary files
superclean: clean
> rm -f *.cod
> rm -f *.ibj
> rm -f *.icd
> rm -f *.lst

takeon: takeon.o
> $(CC) -o takeon takeon.o $(LINK_OPT)

i77.tables.inc: i77.grammar takeon
> ${BINDIR}/takeon i77.grammar=i77.tables.inc,i77.par.debug,i77.lex.debug

pass1: i77.tables.inc icd.utils.o pass1.o
> $(CC) -o pass1 pass1.o icd.utils.o $(LINK_OPT)

pass2: icd.utils.o ibj.utils.o pass2.o
> $(CC) -o pass2 pass2.o icd.utils.o ibj.utils.o $(LINK_OPT)

%.o: %.imp
> ${BINDIR}/imp77 -c -Fc -Fs -Fi $<
>
