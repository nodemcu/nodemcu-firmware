make clean
make all
cd bin/
srec_cat -output full.bin -binary 0x00000.bin -binary -fill 0xff 0x00000 0x10000 0x10000.bin -binary -offset 0x10000
rm ../pre_build/latest/nodemcu_latest.bin
mv full.bin ../pre_build/latest/nodemcu_latest.bin