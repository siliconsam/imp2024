@setlocal
@echo off
@set COM_HOME=%~dp0
@set SOURCE_DIR=%COM_HOME:~0,-1%
@rem just removed the \ (last character) from the path

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
@echo "PASS3 BOOTSTRAP requested"
@echo.
:do_bootstrap
@call :do_c2obj ifreader
@call :do_c2obj writebig
@call :do_c2obj pass3coff -DMSVC
@call :do_c2obj pass3elf  -DMSVC
@call :do_link pass3coff ifreader writebig
@call :do_link pass3elf  ifreader writebig
@goto the_end

:rebuild
@echo.
@echo "PASS3 REBUILD requested"
@echo.
:do_rebuild
@goto do_bootstrap

:install
@echo.
@echo "PASS3 INSTALL requested"
@echo.
:do_install
@copy/y pass3coff.exe   %IMP_INSTALL_HOME%\bin\*
@copy/y pass3elf.exe    %IMP_INSTALL_HOME%\bin\*
@copy/y imp32.bat       %IMP_INSTALL_HOME%\bin\*
@copy/y imp32link.bat   %IMP_INSTALL_HOME%\bin\*
@goto the_end

:clean
@echo.
@echo "PASS3 CLEAN requested"
@echo.
:do_clean
@if exist *.lst del *.lst
@if exist *.map del *.map
@if exist *.obj del *.obj
@if exist *.exe del *.exe
@goto the_end

:superclean
@echo.
@echo "PASS3 SUPERCLEAN requested"
@echo.
:do_superclean
@goto do_clean

:do_c2obj
@set module=%1
@set option=%2
@cl /nologo /Gd /c /Gs /W3 /Od /arch:IA32 -D_CRT_SECURE_NO_WARNINGS /FAscu ^
%option% /Fo%module%.obj /Fa%module%.lst %module%.c
@exit/b

:do_link
@set objlist=%1 %2 %3
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
@echo.
@echo  Legal parameters to the MAKE_LIB script are:
@echo.
@echo     bootstrap:    - pass3coff.exe and pass3elf.exe are created from the various .c source files
@echo.
@echo     rebuild:      - identical behaviour with similar actions to that of the 'bootstrap' parameter
@echo.
@echo     install:      - files released to the binary folder %IMP_INSTALL_HOME%\bin are:
@echo                         - pass3coff.exe  (used to convert .ibj file to a COFF file .obj)
@echo                         - pass3elf.exe   (used to convert .ibj file to a ELF  file .o)
@echo                         - imp32.bar
@echo                         - imp32link.bat
@echo.
@echo     clean:        - all compiler generated files are deleted
@echo.
@echo     superclean:   - identical behaviour with similar actions to that of the 'clean' parameter
@echo.
@echo.
@goto the_end

:the_end
@popd
@endlocal
@exit/b

