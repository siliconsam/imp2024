@setlocal
@echo off
@set COM_HOME=%~dp0
@rem @set IMP_LIB_DIR=%COM_HOME:~0,-1%
@rem just removed the \bin\ (last 5 characters) from the path

@set SOURCE_DIR=%IMP_SOURCE_HOME%\pass3

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
@call :do_c2obj ifreader
@call :do_c2obj writebig
@call :do_c2obj pass3coff -DMSVC
@call :do_c2obj pass3elf  -DMSVC
@call :do_linkn pass3coff ifreader writebig
@call :do_linkn pass3elf  ifreader writebig
@goto the_end

:rebuild
@echo "REBUILD requested"
:do_rebuild
@goto do_bootstrap

:install
@echo "INSTALL requested"
:do_install
copy/y pass3coff.exe   %IMP_INSTALL_HOME%\bin\*
copy/y pass3elf.exe    %IMP_INSTALL_HOME%\bin\*
copy/y imp32.bat       %IMP_INSTALL_HOME%\bin\*
copy/y imp32link.bat   %IMP_INSTALL_HOME%\bin\*
@goto the_end

:clean
@echo "CLEAN requested"
:do_clean
@if exist *.lst del *.lst
@if exist *.map del *.map
@if exist *.obj del *.obj
@if exist *.exe del *.exe
@goto the_end

:superclean
@echo "SUPERCLEAN requested"
:do_superclean
@goto do_clean

:do_c2obj
@set module=%1
@set option=%2
@cl /nologo /Gd /c /Gs /W3 /Od /arch:IA32 -D_CRT_SECURE_NO_WARNINGS /FAscu %option% /Fo%module%.obj /Fa%module%.lst %module%.c
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

@rem This link command line references the C heap library code
@link ^
/nologo ^
/SUBSYSTEM:CONSOLE ^
/stack:0x800000,0x800000 ^
/heap:0x800000,0x800000 ^
/MAPINFO:EXPORTS ^
/MAP:%1.map ^
/OUT:%1.exe ^
%objlist%

@exit/b

:help
:do_help
@echo  Legal parameters to the MAKE_LIB script are:
@echo     bootstrap:    - pass3coff.exe and pass3elf.exe are created from the various .c source files
@echo     rebuild:      - identical behaviour with similar actions to that of the 'bootstrap' parameter
@echo     install:      - files released to the binary folder %IMP_INSTALL_HOME%\bin are:
@echo                         - pass3coff.exe  (used to convert .ibj file to a COFF file .obj)
@echo                         - pass3elf.exe   (used to convert .ibj file to a ELF  file .o)
@echo                         - imp32.bar
@echo                         - imp32link.bat
@echo     clean:        - all compiler generated files are deleted
@echo     superclean:   - identical behaviour with similar actions to that of the 'clean' parameter
@goto the_end

:the_end
@endlocal
@popd
@exit/b

