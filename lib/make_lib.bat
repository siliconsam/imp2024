@setlocal
@echo off
@set COM_HOME=%~dp0
@rem @set IMP_LIB_DIR=%COM_HOME:~0,-1%
@rem just removed the \bin\ (last 5 characters) from the path

@set PERM_HOME=%IMP_INSTALL_HOME%\include
@set P1_HOME=%IMP_INSTALL_HOME%\bin
@set P2_HOME=%IMP_INSTALL_HOME%\bin
@set P3_HOME=%IMP_INSTALL_HOME%\bin

@set SOURCE_DIR=%IMP_SOURCE_HOME%\lib
@set LIB_FILE=libi77.lib

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
@if "%1"=="loadlinux"    @goto loadlinux
@if "%1"=="loadwindows"  @goto loadwindows
@if "%1"=="storewindows" @goto storewindows
@goto help

:bootstrap
@echo "BOOTSTRAP requested"
:do_bootstrap
@set start=ibj
@call :do_makelib
@goto the_end

:rebuild
@echo "REBUILD requested"
:do_rebuild
@set start=imp
@call :do_makelib
@goto the_end

:install
@echo "INSTALL requested"
:do_install
copy/y imprtl-main.obj %IMP_INSTALL_HOME%\lib\*
copy/y libi77.lib      %IMP_INSTALL_HOME%\lib\*
copy/y stdperm.imp     %IMP_INSTALL_HOME%\include\*
@goto the_end

:clean
@echo "CLEAN requested"
:do_clean
@if exist *.cod del *.cod
@if exist *.icd del *.icd
@if exist *.lib del *.lib
@if exist *.lst del *.lst
@if exist *.obj del *.obj
@goto the_end

:superclean
@echo "SUPERCLEAN requested"
:do_superclean
@if exist *.ibj del *.ibj
@goto do_clean

:loadlinux
@echo "LOADLINUX requested"
:do_loadlinux
@set TO_DIR=%SOURCE_DIR%
@set FROM_DIR=%SOURCE_DIR%\linux
@call :do_copyfiles
@goto the_end

:loadwindows
@echo "LOADWINDOWS requested"
:do_loadwindows
@set TO_DIR=%SOURCE_DIR%
@set FROM_DIR=%SOURCE_DIR%\windows
@call :do_copyfiles
@goto the_end

:storewindows
@echo "STOREWINDOWS requested"
:do_storewindows
@set FROM_DIR=%SOURCE_DIR%
@set TO_DIR=%SOURCE_DIR%\windows
@call :do_copyfiles
@goto the_end

:do_copyfiles
@copy/y %FROM_DIR%\implib-heap.ibj %TO_DIR%\*
@copy/y %FROM_DIR%\implib-heap.inc %TO_DIR%\*
@copy/y %FROM_DIR%\implib-trig.ibj %TO_DIR%\*
@copy/y %FROM_DIR%\implib-trig.inc %TO_DIR%\*
@copy/y %FROM_DIR%\imprtl-file.ibj %TO_DIR%\*
@copy/y %FROM_DIR%\imprtl-file.inc %TO_DIR%\*
@copy/y %FROM_DIR%\imprtl-main.ibj %TO_DIR%\*
@copy/y %FROM_DIR%\imprtl-main.imp %TO_DIR%\*
@exit/b

:do_makelib
@call :do_createlib prim-rtl-file
@call :do_appendlib imprtl-main        ignore
@call :do_appendlib impcore-arrayutils lib
@call :do_appendlib impcore-types      lib
@call :do_appendlib impcore-mathutils  lib
@call :do_appendlib impcore-signal     lib
@call :do_appendlib impcore-strutils   lib
@call :do_appendlib implib-arg         lib
@call :do_appendlib implib-debug       lib
@call :do_appendlib implib-env         lib
@call :do_appendlib implib-heap        lib
@call :do_appendlib implib-read        lib
@call :do_appendlib implib-strings     lib
@call :do_appendlib implib-trig        lib
@call :do_appendlib imprtl-check       lib
@call :do_appendlib imprtl-event       lib
@call :do_appendlib imprtl-io          lib
@call :do_appendlib imprtl-file        lib
@call :do_appendlib imprtl-trap        lib
@call :do_appendlib imprtl-line        lib
@call :do_appendlib imprtl-limit       lib
@exit/b

:do_createlib
@set module=%1
@rem Ensure we have a clean library
@if exist %LIB_FILE% del %LIB_FILE%

@rem Compile prim-rtl-file.c (aka %1)
@call :do_c2obj %module% -DMSVC

@rem Store the C source primitives object code into the library
@lib /nologo /out:%LIB_FILE% %module%.obj
@exit/b

:do_appendlib
@set module=%1
@set mode=%2

@call :do_make %module%
@if "%module%"=="imprtl-main" goto :do_appendlib_end
@lib /nologo /out:%LIB_FILE% %LIB_FILE% %module%.obj
:do_appendlib_end
@exit/b

:do_make
@set module=%1

@rem Create the .obj file from the .ibj file
@if "%start%"=="imp" @call :do_imp2ibj %module%
@call :do_ibj2obj %module%
@exit/b

:do_imp2obj
@set module=%1
@call :do_imp2ibj %module%
@call :do_ibj2obj %module%
@exit/b

:do_imp2ibj
@set module=%1
@set file=%SOURCE_DIR%\%module%
@set file=%module%
@%P1_HOME%\pass1.exe %file%.imp,%PERM_HOME%\stdperm.imp=%file%.icd:b,%file%.lst
@%P2_HOME%\pass2.exe %file%.icd:b,%file%.imp=%file%.ibj,%file%.cod
@exit/b

:do_ibj2obj
@set module=%1
@set file=%SOURCE_DIR%\%module%
@set file=%module%
@%P3_HOME%\pass3coff.exe %file%.ibj %file%.obj
@exit/b

:do_c2obj
@set module=%1
@set option=%2
@set file=%SOURCE_DIR%\%module%
@cl /nologo /Gd /c /Gs /W3 /Od /arch:IA32 -D_CRT_SECURE_NO_WARNINGS /FAscu %option% /Fo%file%.obj /Fa%file%.lst %file%.c
@exit/b

:help
:do_help
@echo  Legal parameters to the MAKE_LIB script are:
@echo     bootstrap:    - each ibj file is converted to an obj file by pass3coff.exe
@echo                   - prim-rtl-c is compiled to a .obj file
@echo                   - a library file libi77.a is created from all the .obj files
@echo     rebuild:      - similar actions to that of the 'bootstrap' parameter
@echo                   - except the process starts with the .imp files instead of the .ibj files
@echo     install:      - files released to the library folder %IMP_INSTALL_HOME%\lib are:
@echo                         - the library file libi77.a
@echo                         - the interface file imprtl-main.obj
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
