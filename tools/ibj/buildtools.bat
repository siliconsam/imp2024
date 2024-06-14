@setlocal
@set COM_HOME=%~dp0
@rem now remove the \lib\ (last 10 characters) from the script directory variable
@set BUILD_HOME=%COM_HOME%

@set DEV_HOME=..\..\
@set COMPILER_HOME=%DEV_HOME%\compiler
@set PASS3_HOME=%DEV_HOME%\pass3
@set LIB_HOME=%DEV_HOME%\lib

@rem Tool to read IBJ data in text form and generate an IBJ file
@fpc -gl assemble2ibj.pas
@rem Tool to read a COFF object file and generate debug info
@fpc -gl coff2dump.pas
@rem Tool to read an IBJ file and generate a textual equivalent
@fpc -gl ibj2assemble.pas
@rem Tool to read an IBJ file and generate a COFF file
@rem this is a Pascal version of the C program pass3
@fpc -gl ibj2coff.pas
@rem Tool to read an IBJ file, compact it by eliminating unused external symbols
@fpc -gl ibj2compact.pas

@rem now to build slimibj
@rem Equivalent tool to ibj2compact.pas but written in IMP
@rem Complicated build mechanism since slimibj is split into various IMP77 modules
@call :i32 slimibj
@call :i32 ibjconversion
@call :i32 symbols
@call :i32 labels
@call :dolinkn slimibj ibjconversion symbols labels
@goto the_end

:i32
@set source=%1
@%COMPILER_HOME%\pass1  %source%.imp,%LIB_HOME%\stdperm.imp=%source%.icd:b,%source%.lst
@%COMPILER_HOME%\pass2  %source%.icd:b,%source%.imp=%source%.ibj,%source%.cod
@exit/b

:ibj2obj
@set source=%1
@%PASS3_HOME%\pass3coff %source%.ibj %source%.obj
@exit/b

:dolinkn
@setlocal
@echo off

@echo.
@call :dolinklist %*

@echo **************************
@echo **** ALL LINKING DONE **** for %1
@echo **************************
@goto :end

:dolinklist
@echo off
setlocal enabledelayedexpansion
@echo ********************************************
@echo **** Linking OBJECT files from %*
@echo ********************************************
@echo.

set objlist=
set argCount=0
for %%x in (%*) do (
    @call :ibj2obj %%x
    set /A argCount+=1
    set "objlist=!objlist! %%~x.obj"
)
@echo Number of object files to link: %argCount%
@echo Object link list              : %objlist%
@echo.

@rem This link command line uses the C heap library code
@link ^
/nologo ^
/SUBSYSTEM:CONSOLE ^
/stack:0x800000,0x800000 ^
/heap:0x800000,0x800000 ^
/MAPINFO:EXPORTS ^
/MAP:%1.map ^
/OUT:%1.exe ^
/DEFAULTLIB:%LIB_HOME%\libi77.lib ^
%LIB_HOME%\imprtl-main.obj ^
%objlist% ^
%LIB_HOME%\libi77.lib

@echo.
@endlocal
exit/b

:end
@endlocal
@echo on
@exit/b

:the_end
@endlocal

