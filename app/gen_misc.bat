@echo off
set BACKPATH=%PATH%
set PATH=%BACKPATH%;%CD%\..\tools
@echo on

del /F ..\bin\eagle.app.v6.flash.bin ..\bin\eagle.app.v6.irom0text.bin ..\bin\eagle.app.v6.dump ..\bin\eagle.app.v6.S

cd .output\eagle\debug\image

xt-objdump -x -s eagle.app.v6.out > ..\..\..\..\..\bin\eagle.app.v6.dump
xt-objdump -S eagle.app.v6.out > ..\..\..\..\..\bin\eagle.app.v6.S

xt-objcopy --only-section .text -O binary eagle.app.v6.out eagle.app.v6.text.bin
xt-objcopy --only-section .data -O binary eagle.app.v6.out eagle.app.v6.data.bin
xt-objcopy --only-section .rodata -O binary eagle.app.v6.out eagle.app.v6.rodata.bin
xt-objcopy --only-section .irom0.text -O binary eagle.app.v6.out eagle.app.v6.irom0text.bin

gen_appbin.py eagle.app.v6.out v6

xcopy /y eagle.app.v6.irom0text.bin ..\..\..\..\..\bin\
xcopy /y eagle.app.v6.flash.bin ..\..\..\..\..\bin\

cd ..\..\..\..\

@echo off
set PATH=%BACKPATH%
@echo on
