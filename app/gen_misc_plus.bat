@echo off
set BACKPATH=%PATH%
set PATH=%BACKPATH%;%CD%\..\tools
@echo on

make APP=$1

rm ..\bin\upgrade\%1.bin

cd .output\eagle\debug\image\

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

%OBJCOPY% --only-section .text -O binary eagle.app.v6.out eagle.app.v6.text.bin
%OBJCOPY% --only-section .data -O binary eagle.app.v6.out eagle.app.v6.data.bin
%OBJCOPY% --only-section .rodata -O binary eagle.app.v6.out eagle.app.v6.rodata.bin
%OBJCOPY% --only-section .irom0.text -O binary eagle.app.v6.out eagle.app.v6.irom0text.bin

gen_appbin.py eagle.app.v6.out v6

gen_flashbin.py eagle.app.v6.flash.bin eagle.app.v6.irom0text.bin

cp eagle.app.flash.bin %1.bin

xcopy /y %1.bin ..\..\..\..\..\bin\upgrade\

cd ..\..\..\..\

@echo off
set PATH=%BACKPATH%
@echo on