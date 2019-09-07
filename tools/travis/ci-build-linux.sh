#!/bin/sh

set -e

echo "Running ci build for linux"
(
  cd "$TRAVIS_BUILD_DIR" || exit
  export BUILD_DATE=$(date +%Y%m%d)

  # build integer firmware
  make EXTRA_CCFLAGS="-DLUA_NUMBER_INTEGRAL -DBUILD_DATE='\"'$BUILD_DATE'\"'"
  cd bin/ || exit
  file_name_integer="nodemcu_integer_${TRAVIS_TAG}.bin"
  srec_cat -output ${file_name_integer} -binary 0x00000.bin -binary -fill 0xff 0x00000 0x10000 0x10000.bin -binary -offset 0x10000
  cd ../ || exit

  # build float firmware
  make clean
  make EXTRA_CCFLAGS="-DBUILD_DATE='\"'$BUILD_DATE'\"'" all
  cd bin/ || exit
  file_name_float="nodemcu_float_${TRAVIS_TAG}.bin"
  srec_cat -output ${file_name_float} -binary 0x00000.bin -binary -fill 0xff 0x00000 0x10000 0x10000.bin -binary -offset 0x10000
)
