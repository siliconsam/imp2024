
@set COM_HOME=%~dp0
@set IMP_SOURCE_HOME=%COM_HOME:~0,-1%
@set IMP_INSTALL_HOME=%IMP_SOURCE_HOME%\release
@set dircmd=/ognes

@rem First add in the Pascal compiler (Free Pascal for preference)
@set FPC_HOME=c:\utils\FPC
@set FPC_VERSION=3.2.2
@set FPC_BIN_HOME=%FPC_HOME%\%FPC_VERSION%\bin\i386-win32
@set path=%FPC_BIN_HOME%;%path%

@goto initialise_ok

:initialise_ok

@set path=%IMP_INSTALL_HOME%\bin;%path%
@set libpath=%IMP_INSTALL_HOME%\lib;%libpath%
@title="IMP77 Development Window %IMP_INSTALL_HOME%"

:the_end
@exit/b

