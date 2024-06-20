@setlocal
@echo off
@set COM_HOME=%~dp0
@rem @set IMP_LIB_DIR=%COM_HOME:~0,-1%
@rem just removed the \bin\ (last 5 characters) from the path

@set PERM_HOME=%IMP_INSTALL_HOME%\include
@set P1_HOME=%IMP_INSTALL_HOME%\bin
@set P2_HOME=%IMP_INSTALL_HOME%\bin
@set P3_HOME=%IMP_INSTALL_HOME%\bin
@set BIN_DIR=%IMP_INSTALL_HOME%\bin

@set SOURCE_DIR=%IMP_SOURCE_HOME%\compiler

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
@echo "BOOTSTRAP requested"
:do_bootstrap
@set LIB_HOME=%IMP_SOURCE_HOME%\lib
@set start=ibj
@call :do_makecompiler
@goto the_end

:rebuild
@echo "REBUILD requested"
:do_rebuild
@set LIB_HOME=%IMP_INSTALL_HOME%\lib
@set start=imp
@call :do_makecompiler
@goto the_end

:install
@echo "INSTALL requested"
:do_install
copy/y takeon.exe %IMP_INSTALL_HOME%\bin\*
copy/y pass1.exe  %IMP_INSTALL_HOME%\bin\*
copy/y pass2.exe  %IMP_INSTALL_HOME%\bin\*
@goto the_end

:clean
@echo "CLEAN requested"
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
@echo "SUPERCLEAN requested"
:do_superclean
@if exist *.ibj          del *.ibj
@if exist i77.tables.inc @del i77.tables.inc
@goto do_clean

:do_makecompiler
@call :do_build takeon    %start%
@call :do_linkn takeon
@if not exist i77.tables.inc (
@%BIN_DIR%\takeon.exe i77.grammar,i77.grammar=i77.tables.inc,i77.par.debug,i77.lex.debug
)
@call :do_build ibj.utils %start%
@call :do_build icd.utils %start%
@call :do_build pass1     %start%
@call :do_build pass2     %start%
@call :do_linkn pass1 icd.utils
@call :do_linkn pass2 icd.utils ibj.utils
@exit/b

:do_build
@set module=%1
@set source=%2
@rem Create the .obj file from the .ibj/.imp file
@if "%source%"=="imp" (
    @call :do_imp2ibj %module%
)
@call :do_ibj2obj %module%
@exit/b

:do_imp2obj
@set module=%1
@call :do_imp2ibj %module%
@call :do_ibj2obj %module%
@exit/b

:do_imp2ibj
@set module=%1
@%P1_HOME%\pass1.exe %module%.imp,%PERM_HOME%\stdperm.imp=%module%.icd:b,%module%.lst
@%P2_HOME%\pass2.exe %module%.icd:b,%module%.imp=%module%.ibj,%module%.cod
@exit/b

:do_ibj2obj
@set module=%1
@%P3_HOME%\pass3coff.exe %module%.ibj %module%.obj
@exit/b

:do_linkn
@setlocal
@echo off

@echo.
@call :do_linklist %*

@echo **************************
@echo **** ALL LINKING DONE **** for %1
@echo **************************
@exit/b

:do_linklist
@echo off
setlocal enabledelayedexpansion
@echo ********************************************
@echo **** Linking OBJECT files from %*
@echo ********************************************
@echo.

set objlist=
set argCount=0
for %%x in (%*) do (
@rem    @call :ibj2obj %%x
    set /A argCount+=1
    set "objlist=!objlist! %%~x.obj"
)
@echo Number of object files to link: %argCount%
@echo Object link list              : %objlist%
@echo.

@set HEAP_REQUEST=/heap:0x800000,0x800000
@rem This link command line adds the C heap library code
@rem To include the heap code
@rem - add the line "/heap:0x800000,0x800000 ^" after the "/stack:..." line
@link ^
/nologo ^
/SUBSYSTEM:CONSOLE ^
/stack:0x800000,0x800000 %HEAP_REQUEST% ^
/MAPINFO:EXPORTS ^
/MAP:%1.map ^
/OUT:%1.exe ^
/DEFAULTLIB:%LIB_HOME%\libi77.lib ^
%LIB_HOME%\imprtl-main.obj ^
%objlist% ^
%LIB_HOME%\libi77.lib

@exit/b

:help
:do_help
@echo  Legal parameters to the MAKE_COMPILER script are:
@echo     bootstrap:    - each ibj file is converted to an obj file by pass3coff.exe
@echo                   - the takeon, pass1, pass2 executables are created from the .obj files
@echo                   - and linked using the library file libi77.lib in the .\lib folder
@echo     rebuild:      - similar to bootstrap except the start point is a .imp file
@echo     install:      - the takeon, pass1, pass3 executables are released to the %IMP_INSTALL_HOME%\bin folder
@echo     clean:        - all compiler generated files (except the .ibj files) are deleted
@echo     superclean:   - same as 'clean' except the .ibj files are also deleted
@echo     loadlinux:    - loads the linux specific files from the %IMP_SOURCE_HOME%\lib\linux
@echo     loadwindows:  - loads the windows specific files from the %IMP_SOURCE_HOME%\lib\windows
@echo     storewindows: - stores the windows specific files into the %IMP_SOURCE_HOME%\lib\windows
@goto the_end

:the_end
@endlocal
@popd
@exit/b
