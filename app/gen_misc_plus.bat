@echo off
set BACKPATH=%PATH%
set PATH=%BACKPATH%;%CD%\..\tools
@echo on

rm ..\bin\upgrade\%1.bin

cd .output\eagle\debug\image\

xt-objcopy --only-section .text -O binary eagle.app.v6.out eagle.app.v6.text.bin
xt-objcopy --only-section .data -O binary eagle.app.v6.out eagle.app.v6.data.bin
xt-objcopy --only-section .rodata -O binary eagle.app.v6.out eagle.app.v6.rodata.bin
xt-objcopy --only-section .irom0.text -O binary eagle.app.v6.out eagle.app.v6.irom0text.bin

gen_appbin.py eagle.app.v6.out v6

gen_flashbin.py eagle.app.v6.flash.bin eagle.app.v6.irom0text.bin

cp eagle.app.flash.bin %1.bin

xcopy /y %1.bin ..\..\..\..\..\bin\upgrade\

cd ..\..\..\..\

@echo off
set PATH=%BACKPATH%
@echo on