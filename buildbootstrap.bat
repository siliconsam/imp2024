@setlocal
@echo off
@set COM_HOME=%~dp0
@set IMP_SOURCE_HOME=%COM_HOME:~0,-1%
@set IMP_INSTALL_HOME=%IMP_SOURCE_HOME%\release
@set dircmd=/ognes

@if exist %IMP_INSTALL_HOME%             @rmdir/S/Q %IMP_INSTALL_HOME%

@echo  Creating IMP_INSTALL_HOME folder tree as %IMP_INSTALL_HOME%.
@if not exist %IMP_INSTALL_HOME%         @mkdir %IMP_INSTALL_HOME%
@if not exist %IMP_INSTALL_HOME%\bin     @mkdir %IMP_INSTALL_HOME%\bin
@if not exist %IMP_INSTALL_HOME%\include @mkdir %IMP_INSTALL_HOME%\include
@if not exist %IMP_INSTALL_HOME%\lib     @mkdir %IMP_INSTALL_HOME%\lib

@call %IMP_SOURCE_HOME%\pass3\make_pass3 bootstrap
@call %IMP_SOURCE_HOME%\pass3\make_pass3 install

@call %IMP_SOURCE_HOME%\lib\make_lib loadwindows
@call %IMP_SOURCE_HOME%\lib\make_lib bootstrap
@call %IMP_SOURCE_HOME%\lib\make_lib install

@rem the bootstrap of the syntax table generator (takeon)
@rem and the compiler (pass1, pass2) always uses
@rem the run-time library in the %IMP_SOURCE_HOME%\lib folder
@call %IMP_SOURCE_HOME%\compiler\make_compiler bootstrap
@call %IMP_SOURCE_HOME%\compiler\make_compiler install

@call %IMP_SOURCE_HOME%\compiler\make_compiler clean
@call %IMP_SOURCE_HOME%\lib\make_lib clean
@call %IMP_SOURCE_HOME%\pass3\make_pass3 clean
