.RECIPEPREFIX = >

CC = gcc ${M32}
CCFLAGS = -O

BASEDIR = ${IMP_INSTALL_HOME}
BINDIR = ${BASEDIR}/bin

SRC=pass3coff.c \
    pass3elf.c \
    ifreader.c \
    writebig.c

# Default make target
all: pass3coff pass3elf
> @echo "Completed pass3 make ALL"

# We need to build pass1,pass2 from their .o files (created by the cross build script make.bat)
# Also build pass3 completely from source
#bootstrap: pass3coff pass3elf ld.i77.script imp77 imp77link
bootstrap: install
> @echo "Completed pass3 make BOOTSTRAP"
## Just install the programs
#> install -t $(BINDIR) pass3coff
#> install -t $(BINDIR) pass3elf
#> install -t $(BINDIR) ld.i77.script
#> install -t $(BINDIR) imp77
#> install -t $(BINDIR) imp77link

rebuild: pass3coff pass3elf
> @echo "Completed pass3 make REBUILD"

# We need to build pass1, pass2 and pass3
install: pass3coff pass3elf
# Now install the programs
> @install -t $(BINDIR) pass3coff
> @install -t $(BINDIR) pass3elf
> @install -t $(BINDIR) ld.i77.script
> @install -t $(BINDIR) imp77
> @install -t $(BINDIR) imp77link
> @echo "Completed pass3 make INSTALL"

# do a minimal tidy up of programs and temporary files
clean: #
> @rm -f pass3elf
> @rm -f pass3coff
> @rm -f *.o
> @echo "Completed pass3 make CLEAN"

# really scrub away all programs and temporary files
superclean: clean
> @rm -f *.lst
> @echo "Completed pass3 make SUPERCLEAN"

pass3elf: pass3elf.o ifreader.o writebig.o
> @$(CC) -o pass3elf pass3elf.o ifreader.o writebig.o
> @echo "Completed pass3 make PASS3ELF"

pass3coff: pass3coff.o ifreader.o writebig.o
> @$(CC) -o pass3coff pass3coff.o ifreader.o writebig.o
> @echo "Completed pass3 make PASS3COFF"

%.o: %.c
> @$(CC) -c $(CCFLAGS) $<
