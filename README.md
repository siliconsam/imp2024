"# imp2024"
IMP-77 for WSL (Windows sub-system for Linux), UNIX/Linux
This version is labeled IMP2024 (but still implements the IMP-77 language)

This version enhances the IMP2022 version of the IMP-77 compiler.
Note that this Git repository only contains the IMP2024 compiler.
The documentation, examples and tools code are now in separate Git repositories.

The documentation is now in the "imp_documents" Git repository.
Imp code examples are now in the "imp_examples" Git repository.
Imp tools source is now in the "imp_tools" Git repository.

Enhancements:
    1) The run-time library reports line numbers and source module names in stack trace
       triggered by an IMP signal that has no event handler.

This distribution contains:-
    1) source code of the IMP compiler + run-time library
        (for Intel 386 UNIX/Linux machines that use ELF object files.)
    2) Source code (pass3coff.c) to generate Windows COFF object files


COMMON PRE-REQUISITES

* Copy of the IMP2024 git repository on your Windows/Linux/WSL machine

PRE-REQUISITES FOR LINUX

* Ensure the Linux system has the latest software
    * sudo apt update   (or equivalent Linux command)
    * sudo apt upgrade  (or equivalent Linux command)
* dos2unix
    * sudo apt install dos2unix (or equivalent command)
* A C compiler and libraries. (Build tested against the gcc compiler suite)
    * Ensure the C compiler can compile/link 32-bit code
    * sudo apt install gcc-multilib  (for the gcc compiler)
* Read, Write + executable access to the /usr/local folder and sub-folders


PRE-REQUISITES FOR WINDOWS (Versions 8 upwards)

* A C compiler + libraries. (Build tested with Visual Studio in command-line mode)


POSSIBLE REQUIRED TWEAKS FOR COMPILER INSTALLATION:

The Makefiles supplied don't try to figure out local installed software or
policies, so you may need to make some changes.  In particular:

1.  The Makefiles assume GCC is your compiler.
2.  The Makefiles, and the shell/make scripts assume that you will install to
    /usr/local/bin, /usr/local/lib and /usr/local/include.
3.  The "install" command is particularly sensitive to the UNIX/Linux variant you
    are running, and the install section of the Makefiles will likely need fixing.
4.  The loader script "ld.i77.script" in "imp2024/pass3" ass-u-mes
    that ld will concatenate this script into the default GCC loader script.
    This ld.i77.script is ESSENTIAL.
    If not then
        * run "ld --verbose"
        * copy the generated ld script into a file "compiler/ld.script"
        * insert the "section" contents of ld.i77.script before the .data
          instructions into the "compiler/ld.script"
        * rename the "compiler/ld.script" to be "compiler/ld.i77.script"
    ***** This potential amendment to ld.i77.script MUST be done BEFORE running the "make bootstrap" in the compiler folder.

    Imp-77 event handling depends on the individual event traps being in
    one executable section ".trap" in the order of ".imp.trap.b", all ".imp.trap.d" object file sections
    and finally the ".imp.trap.f" object file section.

    Imp-77 line v address code depends on the individual module line data being in
    one executable section ".lines" formed from the "imp.line.B" object sections, then all "imp.line.D" sections
    and finally the ".imp.line.F" object file section.

    The Windows linker does this automatically
    Versions of the GCC binutils loader ld (upto 2.27) under WSL/Linux seem ok!
    Versions of the GCC binutils loader ld (after 2.27) have problems with relocation records

The Linux version is slightly different to the Windows version.
COFF object file symbols for Windows have a _ as the first character of the symbol name.
Linux/ELF symbols do not have _ as their first character.
The bootstrap files provided are for the Linux/WSL version.


BOOTSTRAPPING

You don't necessarily have an IMP compiler already installed.
So the library and compiler directories contains
    1) the IMP source files
    2) the IMP intermediate files (.ibj) generated by pass2 from the sources.
    3) some utilities written in the Free Pascal dialect to analyse
When packed the archive, the source and .ibj files are up-to-date, so "make" should only
need to compile the C portions, and link the object files.
Be aware of the possible tweaks mentioned above.

The order of bootstrapping is...

    * build the pass3 program (used to generate the Elf .o files from .ibj files)
        The pass3elf program is compiled from the pass3elf.c source (generates .rel relocations)
        N.B. the pass3coff.c program generates Windows COFF files from the same .ibj input.

    * cd imp2024/pass3
    * make install

    * # build the library
	* cd ../lib
	* make bootstrap

    * # build the pass1, pass2 programs
	* cd ../compiler
    * make bootstrap

This should have installed the IMP compiler and libraries
Next do a general tidy-up of the temporary files
    * cd ../pass3
    * make superclean
    * cd ../lib
    * make superclean
    * cd ../compiler
    * make superclean

I strongly suggest you then make copies of:-
    * the installed compiler (ass-u-med in /usr/local/bin)
    * installed library (/usr/local/bin)
    * installed include files. (usr/local/include)
    * the ../compiler folder
    * the ../lib folder

Let me know if this bootstrapping step doesn't work (via a github notification),
but only after you have tried all of the above installation "tweaks".
Bootstrapping has been tested in:-
 1) a WSL version 2 environment (Debian and Ubuntu 22.04 LTS)
    These environments use a later version of the GNU binutils package
 2) a Centos-7 virtual machine
    This uses an earlier version of the GNU binutils package

Both the Centos + WSL environments needed tweaks to the default ld loader script.
These are already located in the ld.i77.script.
This script only copes with .rel relocation type.

The pass3elf.c code generates .rel relocation entities.

Currently the combination of pass3elf and ld.i77.script will generate working executables.
However the creation of a shareable library (for the IMP RTL library code) generates 2 warnings:
1) relocation in read-only section '.text' 
2) creating DT_TEXTREL in a PIE

N.B. The "make bootstrap" command uses dos2unix to change the line-endings of various
text files to have the UNIX/Linux CR line ending rather than Windows CR-LF line ending.
The other make commands ass-u-me that text files have the CR line ending.

USER ENHANCEMENTS

Once the initial bootstrap generation of the IMP library and compiler programs is complete then
you can start to "enhance" the IMP compiler by modifying the source files in the compiler and lib folders.

Then to re-build the libraries and compiler
    * cd ../lib
    * make
    * cd ../compiler
    * make

To verify the re-built (but un-installed) compilers and libraries,
use the various imp examples in the "imp_tests" Git repository
Specify the -e option (== testmode) to the imp77 script when compiling a single
IMP-77 file, so that the un-installed compiler and libraries are used.

For multi-file IMP-77 programs you can use the imp77link script, however this script
only uses the installed compiler and libraries.

If there are no errors and you feel confident with your new "enhancements"
Then to install the new version of compiler and libraries
    * cd ../lib
    * make install
    * cd ../compiler
    * make install

Remember, always keep backups of the previous versions of source files and the
installed files.
If not, the GitHub repository should contain a set of files that can be used to
bootstrap the IMP compiler.

RUNNING THE IMP COMPILER
The compiler is invoked by the imp77 shell script.
The various options are:
    * -c  This just generates the ELF object file (no linking to an executable)
    * -Fc Generates a .cod file which lists the code generated by the compiler
    * -Fs Generates a .lst file which indicates any syntax errors found
    * -Fi retains the .ibj, .icd, .o files generated by the compiler
    * The .imp extension of the source file must be given
    *
    * The options can be combined.
    * e.g. imp77 -c -Fc -Fs -Fi pass2.imp
This retains all the intermediate files and generates the pass2 object file with
the .cod, .lsr, .ibj, .icd also being retained.

There is an additional script imp77link which can take an IMP program split
into several Imp source files and individually generate the ELF object files
before linking the ELF .o files into an executable.
Examples of the use of imp77link are in various Makefile files
1) in the compiler folder to build pass2
2) in the tools/ibj folder to build slimibj

EXERCISE FOR USER
As an exercise, create a new script "imp77elink" which uses the un-installed/testmode
compiler and libraries 
 
UTILITIES
The Git repository "imp_tools" contains various utilities (in IMP and Free Pascal)
These help to analyse the intermediate files (.icd, .ibj) generated by the compiler suite.
The pass1 executable generates the .icd intermediate files.
The pass2 executable generates the .ibj intermediate files.

BUILDING WINDOWS VERSION

Obtain the pre-requisite software for Windows

1) Copy/Git pull the git repository folder tree to a Windows folder
    (or obtain a .zip file of the folder source and unzip to a folder of your choice)
2) Run the buildwindows.bat script inside a command shell with access to the Visual Studio
   32-bit command line C compiler and linker.
3) This generates a release folder tree with a Windows version of the IMP compiler
4) Modify the setenv.bat script to point to the correct version of the Free Pascal compiler
5) When using the Visual Studio 32-bit command shell ensure you ALWAYS call setenv.bat
    This will give access to the IMP compiler and associated utilities.

Good luck and enjoy!

Original implementation by:
Andy Davis
andy@nb-info.co.uk

Refreshed and enhanced by:
John McMullin
jdmcmullin@aol.com

