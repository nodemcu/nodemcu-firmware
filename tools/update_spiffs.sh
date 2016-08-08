#!/bin/sh

# This script updates the SPIFFS code from the master repository

if [ ! -e ../tools/update_spiffs.sh ]; then
  echo Must run from the tools directory
  exit 1
fi

git clone https://github.com/pellepl/spiffs

cp spiffs/src/*.[ch] ../app/spiffs
cp spiffs/LICENSE ../app/spiffs

rm -fr spiffs
