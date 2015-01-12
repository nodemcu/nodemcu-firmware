#!/bin/bash

export PATH=/home/supermap/OpenThings/esp-open-sdk/xtensa-lx106-elf/bin:$PATH
echo $PATH

echo "========================================"
echo "Building nodemcu-firmware for ESP8266..."
echo "========================================"

cd nodemcu-firmware
echo "clean..."
#make clean
make

APPOUT="./app/.output/eagle/debug/image/eagle.app.v6.out"
esptool -eo $APPOUT -bo ./firmware/0x00000.bin -bs .text -bs .data -bs .rodata -bc -ec
esptool -eo $APPOUT -es .irom0.text ./firmware/0x10000.bin -ec

CWD="/firmware/esp.nodemcu.bin"
OUTPUTBIN=$(pwd)$CWD

FILE_00000="./firmware/0x00000.bin"
FILE_10000="./firmware/0x10000.bin"
FILE_7C000="./../esp-open-sdk/esp_iot_sdk_v0.9.5/bin/esp_init_data_default.bin"
FILE_7E000="./../esp-open-sdk/esp_iot_sdk_v0.9.5/bin/blank.bin"

FILE_00000_SIZE=$(stat -c%s $FILE_00000)
FILE_10000_SIZE=$(stat -c%s $FILE_10000)
FILE_7C000_SIZE=$(stat -c%s $FILE_7C000)
FILE_7E000_SIZE=$(stat -c%s $FILE_7E000)

echo "build blank flash image"
IMAGESIZE=$((0x7E000 + FILE_7E000_SIZE))

#thank igr for this tip
dd if=/dev/zero bs=1 count="$IMAGESIZE" conv=notrunc | LC_ALL=C tr "\000" "\377" > "$OUTPUTBIN"

echo "patch image with bins"
dd if="$FILE_00000" of="$OUTPUTBIN" bs=1 seek=0 count="$FILE_00000_SIZE" conv=notrunc
echo "patched FILE_00000"

JUMP=$((0x10000))
dd if="$FILE_10000" of="$OUTPUTBIN" bs=1 seek="$JUMP" count="$FILE_10000_SIZE" 
conv=notrunc
echo "patched FILE_10000"

JUMP=$((0x7C000))
dd if="$FILE_7C000" of="$OUTPUTBIN" bs=1 seek="$JUMP" count="$FILE_7C000_SIZE" 
conv=notrunc  
echo "patched FILE_7C000"

JUMP=$((0x7E000))
dd if="$FILE_7E000" of="$OUTPUTBIN" bs=1 seek="$JUMP" count="$FILE_7E000_SIZE" conv=notrunc  
echo "patched FILE_7E000"
 
echo ">>build image [nodemcu] finished."
echo "Output To: "$OUTPUTBIN

cd ../../
