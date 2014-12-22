#!/bin/bash -x
touch user/user_main.c
make APP=$1
if [ $? == 0 ];then
rm ../bin/upgrade/user$1.bin ../bin/upgrade/user$1.dump ../bin/upgrade/user$1.S

cd .output/eagle/debug/image/

xt-objdump -x -s eagle.app.v6.out > ../../../../../bin/upgrade/user$1.dump
xt-objdump -S eagle.app.v6.out > ../../../../../bin/upgrade/user$1.S

xt-objcopy --only-section .text -O binary eagle.app.v6.out eagle.app.v6.text.bin
xt-objcopy --only-section .data -O binary eagle.app.v6.out eagle.app.v6.data.bin
xt-objcopy --only-section .rodata -O binary eagle.app.v6.out eagle.app.v6.rodata.bin
xt-objcopy --only-section .irom0.text -O binary eagle.app.v6.out eagle.app.v6.irom0text.bin

../../../../../tools/gen_appbin.py eagle.app.v6.out v6

../../../../../tools/gen_flashbin.py eagle.app.v6.flash.bin eagle.app.v6.irom0text.bin

cp eagle.app.flash.bin user$1.bin
cp user$1.bin ../../../../../bin/upgrade/

cd ../../../../../

else
echo "make error"
fi
