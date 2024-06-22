@setlocal
@echo off
@set COM_HOME=%~dp0
@set SOURCE_DIR=%COM_HOME:~0,-1%
@rem just removed the \ (last character) from the path

@set PERM_HOME=%IMP_INSTALL_HOME%\include
@set BIN_DIR=%IMP_INSTALL_HOME%\bin
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
@echo.
@echo "LIBRARY BOOTSTRAP requested"
@echo.
:do_bootstrap
@rem create the libi77 library from the only C source module
@call :do_createlib prim-rtl-file -DMSVC
@rem compile the IMP inteface module
@call :do_compile   imprtl-main   ibj  nolib
@rem start with the imp run-time module ibj files
@rem create the corresponding COFF obj files
@rem then populate the libi77 library with the generated obj files
@call :do_loadlib ibj
@goto the_end

:rebuild
@echo.
@echo "LIBRARY REBUILD requested"
@echo.
:do_rebuild
@rem create the libi77 library from the only C source module
@call :do_createlib prim-rtl-file -DMSVC
@rem compile the IMP inteface module
@call :do_compile   imprtl-main   imp  nolib
@rem start with the imp run-time module imp source files
@rem to form the corresponding ibj files
@rem create the corresponding COFF obj files
@rem then populate the libi77 library with the generated obj files
@call :do_loadlib imp
@goto the_end

:install
@echo.
@echo "LIBRARY INSTALL requested"
@echo.
:do_install
@copy/y imprtl-main.obj %IMP_INSTALL_HOME%\lib\*
@copy/y libi77.lib      %IMP_INSTALL_HOME%\lib\*
@copy/y stdperm.imp     %IMP_INSTALL_HOME%\include\*
@goto the_end

:clean
@echo.
@echo "LIBRARY CLEAN requested"
@echo.
:do_clean
@if exist *.cod del *.cod
@if exist *.icd del *.icd
@if exist *.lib del *.lib
@if exist *.lst del *.lst
@if exist *.obj del *.obj
@goto the_end

:superclean
@echo.
@echo "LIBRARY SUPERCLEAN requested"
@echo.
:do_superclean
@if exist *.ibj del *.ibj
@goto do_clean

:loadlinux
@echo.
@echo "LOADLINUX requested"
@echo.
:do_loadlinux
@call :do_copyfiles %SOURCE_DIR%\linux %SOURCE_DIR%
@goto the_end

:loadwindows
@echo.
@echo "LOADWINDOWS requested"
@echo.
:do_loadwindows
@call :do_copyfiles %SOURCE_DIR%\windows %SOURCE_DIR%
@goto the_end

:storewindows
@echo.
@echo "STOREWINDOWS requested"
@echo.
:do_storewindows
@call :do_copyfiles %SOURCE_DIR% %SOURCE_DIR%\windows
@goto the_end

:do_copyfiles
@set from_dir=%1
@set to_dir=%2
@copy/y %FROM_DIR%\implib-heap.ibj %TO_DIR%\*
@copy/y %FROM_DIR%\implib-heap.inc %TO_DIR%\*
@copy/y %FROM_DIR%\implib-trig.ibj %TO_DIR%\*
@copy/y %FROM_DIR%\implib-trig.inc %TO_DIR%\*
@copy/y %FROM_DIR%\imprtl-file.ibj %TO_DIR%\*
@copy/y %FROM_DIR%\imprtl-file.inc %TO_DIR%\*
@copy/y %FROM_DIR%\imprtl-main.ibj %TO_DIR%\*
@copy/y %FROM_DIR%\imprtl-main.imp %TO_DIR%\*
@exit/b

:do_loadlib
@rem compile the various library modules
@rem add them to the libi77 library
@set start=%1

@rem compile the implib-XXX modules
@for %%a in (arg,debug,env,heap,read,strings,trig) do (
    @call :do_compile "implib-%%a" %start%  lib
)

@rem compile the imprtl-XXX modules
@for %%a in (check,event,io,file,trap,line,limit) do (
    @call :do_compile "imprtl-%%a" %start%  lib
)

@rem compile the impcore-XXX modules
@for %%a in (arrayutils,types,mathutils,signal,strutils) do (
    @call :do_compile "impcore-%%a" %start%  lib
)
@exit/b

:do_createlib
@rem we create the library from the only source file written in C
@set module=%1
@set option=%2

@rem compile the C source
@cl /nologo /Gd /c /Gs /W3 /Od /arch:IA32 -D_CRT_SECURE_NO_WARNINGS /FAscu ^
%option% /Fo%module%.obj /Fa%module%.lst %module%.c

@rem Ensure we have a clean library
@if exist %LIB_FILE% del %LIB_FILE%
@rem Store the C source primitives object code into the library
@lib /nologo /out:%LIB_FILE% %module%.obj
@exit/b

:do_compile
@rem compile the specified IMP module
@set module=%1
@set start=%2
@set append=%3

@rem create the .ibj file if starting from the .imp source
@if "%start%"=="imp" (
    @rem We can assume that the pass1,pass2 executables are in the "release" folder
    @rem Create the .ibj file from the .imp source file
    @%BIN_DIR%\pass1.exe %module%.imp,%PERM_HOME%\stdperm.imp=%module%.icd:b,%module%.lst
    @%BIN_DIR%\pass2.exe %module%.icd:b,%module%.imp=%module%.ibj,%module%.cod
)
@rem we assume that the appropriate pass3XXX.exe is always in the "release" folder
@rem Create the .obj file from the .ibj file
@%BIN_DIR%\pass3coff.exe %module%.ibj %module%.obj

@rem do we need to insert the objet file into the libi77 library?
@if "%append%"=="lib" (
    @lib /nologo /out:%LIB_FILE% %LIB_FILE% %module%.obj
)
@exit/b

:help
:do_help
@echo.
@echo  Legal parameters to the MAKE_LIB script are:
@echo.
@echo     bootstrap:    - each ibj file is converted to an obj file by pass3coff.exe
@echo                   - prim-rtl-c is compiled to a .obj file
@echo                   - a library file libi77.a is created from all the .obj files
@echo.
@echo     rebuild:      - similar actions to that of the 'bootstrap' parameter
@echo                   - except the process starts with the .imp files instead of the .ibj files
@echo.
@echo     install:      - files released to the library folder %IMP_INSTALL_HOME%\lib are:
@echo                         - the library file libi77.a
@echo                         - the interface file imprtl-main.obj
@echo.
@echo     clean:        - all compiler generated files (except the .ibj files) are deleted
@echo.
@echo     superclean:   - same as 'clean' except the .ibj files are also deleted
@echo.
@echo     loadlinux:    - loads the O/S specific files from the %SOURCE_HOME%\linux
@echo.
@echo     loadwindows:  - loads the O/S specific files from the %SOURCE_HOME%\windows
@echo.
@echo     storewindows: - stores the Windows O/S specific files into the %SOURCE_HOME%\windows
@echo                         - Don't mix loadlinux then storewindows 
@echo.
@echo.
@goto the_end

:the_end

@popd
@endlocal
@exit/b
