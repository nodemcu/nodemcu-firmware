@echo off
set BACKPATH=%PATH%
set PATH=%BACKPATH%;%CD%\..\tools
@echo on

make %1

del /F ..\bin\eagle.app.v6.flash.bin ..\bin\eagle.app.v6.irom0text.bin ..\bin\eagle.app.v6.dump ..\bin\eagle.app.v6.S

cd .output\eagle\debug\image

@echo off
set OBJDUMP=xt-objdump 
set OBJCOPY=xt-objcopy
if defined XTENSA_CORE (
	set OBJDUMP=xt-objdump 
	set OBJCOPY=xt-objcopy
) else (
	set OBJDUMP=xtensa-lx106-elf-objdump 
	set OBJCOPY=xtensa-lx106-elf-objcopy
)
@echo on

%OBJDUMP% -x -s eagle.app.v6.out > ..\..\..\..\..\bin\eagle.app.v6.dump
%OBJDUMP% -S eagle.app.v6.out > ..\..\..\..\..\bin\eagle.app.v6.S

%OBJCOPY% --only-section .text -O binary eagle.app.v6.out eagle.app.v6.text.bin
%OBJCOPY% --only-section .data -O binary eagle.app.v6.out eagle.app.v6.data.bin
%OBJCOPY% --only-section .rodata -O binary eagle.app.v6.out eagle.app.v6.rodata.bin
%OBJCOPY% --only-section .irom0.text -O binary eagle.app.v6.out eagle.app.v6.irom0text.bin

gen_appbin.py eagle.app.v6.out v6

xcopy /y eagle.app.v6.irom0text.bin ..\..\..\..\..\bin\
xcopy /y eagle.app.v6.flash.bin ..\..\..\..\..\bin\

cd ..\..\..\..\

@echo off
set PATH=%BACKPATH%
@echo on
