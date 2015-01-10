@echo off
set BACKPATH=%PATH%
set PATH=%BACKPATH%;%CD%\..\tools
@echo on

rem gen_misc.bat
rem eagle.app.v6.flash.bin: 0x00000
rem eagle.app.v6.irom0text.bin: 0x10000
rem esp_init_data_default.bin: 0x7c000
rem blank.bin: 0x7e000

cd ..\bin
esptool.py -p com1 write_flash 0x00000 eagle.app.v6.flash.bin 0x10000 eagle.app.v6.irom0text.bin 0x7c000 esp_init_data_default.bin 0x7e000 blank.bin
echo ************************* flash end *************************************
cd ..\app

@echo off
set PATH=%BACKPATH%
@echo on
