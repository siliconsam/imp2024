set LIB_HOME=c:\imp2022\release\lib
@call imp32 -Fc -Fs -Fi -Fh slimibj
@call slimibj.exe slimibj.ibj=thin.ibj > slimibj00.lst

@call copy thin.ibj slimibj.ibj
@call pass3coff slimibj.ibj
@call link /nologo /SUBSYSTEM:CONSOLE /stack:0x800000,0x800000 /heap:0x800000,0x800000 /MAPINFO:EXPORTS /MAP:slimibj.map /OUT:slimibj.exe /DEFAULTLIB:%LIB_HOME%\libi77.lib %LIB_HOME%\imprtl-main.obj slimibj.obj %LIB_HOME%\libi77.lib
@call slimibj.exe slimibj.ibj=thin.ibj > slimibj01.lst

@call copy thin.ibj slimibj.ibj
@call pass3coff slimibj.ibj
@call link /nologo /SUBSYSTEM:CONSOLE /stack:0x800000,0x800000 /heap:0x800000,0x800000 /MAPINFO:EXPORTS /MAP:slimibj.map /OUT:slimibj.exe /DEFAULTLIB:%LIB_HOME%\libi77.lib %LIB_HOME%\imprtl-main.obj slimibj.obj %LIB_HOME%\libi77.lib
@call slimibj.exe slimibj.ibj=thin.ibj > slimibj02.lst
