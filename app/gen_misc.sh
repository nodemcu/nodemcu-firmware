#!/bin/bash -x
make
if [ $? == 0 ];then
rm ../bin/eagle.app.v6.flash.bin ../bin/eagle.app.v6.irom0text.bin ../bin/eagle.app.v6.dump ../bin/eagle.app.v6.S

cd .output/eagle/debug/image

xtensa-lx106-elf-objdump -x -s eagle.app.v6.out > ../../../../../bin/eagle.app.v6.dump
xtensa-lx106-elf-objdump -S eagle.app.v6.out > ../../../../../bin/eagle.app.v6.S

xtensa-lx106-elf-objcopy --only-section .text -O binary eagle.app.v6.out eagle.app.v6.text.bin
xtensa-lx106-elf-objcopy --only-section .data -O binary eagle.app.v6.out eagle.app.v6.data.bin
xtensa-lx106-elf-objcopy --only-section .rodata -O binary eagle.app.v6.out eagle.app.v6.rodata.bin
xtensa-lx106-elf-objcopy --only-section .irom0.text -O binary eagle.app.v6.out eagle.app.v6.irom0text.bin

../../../../../tools/gen_appbin.py eagle.app.v6.out v6

cp eagle.app.v6.irom0text.bin ../../../../../bin/
cp eagle.app.v6.flash.bin ../../../../../bin/

cd ../../../../../

else
echo "make error"
fi
