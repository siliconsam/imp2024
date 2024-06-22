@setlocal
@echo off
@set COM_HOME=%~dp0
@set SOURCE_DIR=%COM_HOME:~0,-1%
@rem just removed the \ (last character) from the path

@rem always use the bootstrap/rebuild library
@set LIB_HOME=%IMP_SOURCE_HOME%\lib

@set PERM_HOME=%IMP_INSTALL_HOME%\include
@set BIN_DIR=%IMP_INSTALL_HOME%\bin

@pushd %SOURCE_DIR%

:parseargs
@if "%1"==""             @goto help
@if "%1"=="help"         @goto help
@if "%1"=="/h"           @goto help
@if "%1"=="-h"           @goto help
@if "%1"=="bootstrap"    @goto bootstrap
@if "%1"=="rebuild"      @goto rebuild
@if "%1"=="install"      @goto install
@if "%1"=="clean"        @goto clean
@if "%1"=="superclean"   @goto superclean
@goto help

:bootstrap
@echo.
@echo "COMPILER BOOTSTRAP requested"
@echo.
:do_bootstrap
@call :do_makecompiler ibj
@goto the_end

:rebuild
@echo.
@echo "COMPILER REBUILD requested"
@echo.
:do_rebuild
@call :do_makecompiler imp
@goto the_end

:install
@echo.
@echo "COMPILER INSTALL requested"
@echo.
:do_install
@copy/y takeon.exe %IMP_INSTALL_HOME%\bin\*
@copy/y pass1.exe  %IMP_INSTALL_HOME%\bin\*
@copy/y pass2.exe  %IMP_INSTALL_HOME%\bin\*
@goto the_end

:clean
@echo.
@echo "COMPILER CLEAN requested"
@echo.
:do_clean
@if exist *.cod   del *.cod
@if exist *.debug del *.debug
@if exist *.exe   del *.exe
@if exist *.icd   del *.icd
@if exist *.lst   del *.lst
@if exist *.map   del *.map
@if exist *.obj   del *.obj
@goto the_end

:superclean
@echo.
@echo "COMPILER SUPERCLEAN requested"
@echo.
:do_superclean
@if exist *.ibj          del *.ibj
@if exist i77.tables.inc @del i77.tables.inc
@goto do_clean

:do_makecompiler
@set start=%1

@rem compile the utility code
@for %%a in (takeon,ibj.utils,icd.utils) do (
    @call :do_compile "%%a" %start%
)
@call :do_link takeon

@rem check if we need to recreate the language table data
@if "%start%"=="imp" @if not exist i77.tables.inc (
    @takeon.exe i77.grammar,i77.grammar=i77.tables.inc,i77.par.debug,i77.lex.debug
)

@rem finally compile and link the language syntax recogniser (pass1)
@for %%a in (pass1,pass2) do (
    @call :do_compile "%%a" %start%
)

@rem call :do_compile pass1     %start%
@rem call :do_compile pass2     %start%
@call :do_link pass1 icd.utils
@call :do_link pass2 icd.utils ibj.utils
@exit/b

:do_compile
@set module=%1
@set source=%2
@rem Create the .obj file from the .ibj/.imp file
@if "%source%"=="imp" (
    @%BIN_DIR%\pass1.exe %module%.imp,%PERM_HOME%\stdperm.imp=%module%.icd:b,%module%.lst
    @%BIN_DIR%\pass2.exe %module%.icd:b,%module%.imp=%module%.ibj,%module%.cod
)
@%BIN_DIR%\pass3coff.exe %module%.ibj %module%.obj
@exit/b

:do_link
@set file1=%1
@set file2=%2
@set file3=%3
@set file4=%4
@if not "%file1%"=="" @set objlist=%file1%.obj
@if not "%file2%"=="" @set objlist=%file1%.obj %file2%.obj
@if not "%file3%"=="" @set objlist=%file1%.obj %file2%.obj %file3%.obj
@if not "%file4%"=="" @set objlist=%file1%.obj %file2%.obj %file3%.obj %file4%.obj

@set HEAP_REQUEST=/heap:0x800000,0x800000
@rem This link command line adds the C heap library code
@rem To include the heap code
@rem - add the line "/heap:0x800000,0x800000 ^" after the "/stack:..." line
@link /nologo /SUBSYSTEM:CONSOLE /stack:0x800000,0x800000 %HEAP_REQUEST% ^
/MAPINFO:EXPORTS /MAP:%1.map /OUT:%1.exe ^
/DEFAULTLIB:%LIB_HOME%\libi77.lib %LIB_HOME%\imprtl-main.obj %objlist% ^
%LIB_HOME%\libi77.lib

@exit/b

:help
:do_help
@echo.
@echo  Legal parameters to the MAKE_COMPILER script are:
@echo.
@echo     bootstrap:    - each ibj file is converted to an obj file by pass3coff.exe
@echo                   - the takeon, pass1, pass2 executables are created from the .obj files
@echo                   - and linked using the library file libi77.lib in the .\lib folder
@echo.
@echo     rebuild:      - similar to bootstrap except the start point is a .imp file
@echo.
@echo     install:      - the takeon, pass1, pass3 executables are released to the %IMP_INSTALL_HOME%\bin folder
@echo.
@echo     clean:        - all compiler generated files (except the .ibj files) are deleted
@echo.
@echo     superclean:   - same as 'clean' except the .ibj files are also deleted
@echo.
@echo.
@goto the_end

:the_end
@popd
@endlocal
@exit/b
