#!/bin/sh

set -e

echo "Running PR build (all modules, SSL enabled, debug enabled)"
(
cd "$TRAVIS_BUILD_DIR"/app/include || exit
# uncomment disabled modules e.g. '//#define LUA_USE_MODULES_UCG' -> '#define LUA_USE_MODULES_UCG'
sed -E -i.bak 's@(//.*)(#define *LUA_USE_MODULES_.*)@\2@g' user_modules.h
cat user_modules.h

# enable SSL
sed -i.bak 's@//#define CLIENT_SSL_ENABLE@#define CLIENT_SSL_ENABLE@' user_config.h

# enable debug
sed -E -i.bak 's@// ?#define DEVELOP_VERSION@#define DEVELOP_VERSION@' user_config.h

# enable FATFS
sed -i 's@//#define BUILD_FATFS@#define BUILD_FATFS@' user_config.h
cat user_config.h

cd "$TRAVIS_BUILD_DIR"/ld || exit
 # increase irom0_0_seg size for all modules build
 sed -E -i.bak 's@(.*irom0_0_seg *:.*len *=) *[^,]*(.*)@\1 0x200000\2@' nodemcu.ld
 sed -E -i.bak 's@(.*iram1_0_seg *:.*len *=) *[^,]*(.*)@\1 0x100000\2@' nodemcu.ld
 cat nodemcu.ld

# change to "root" directory no matter where the script was started from
cd "$TRAVIS_BUILD_DIR" || exit
make clean
make

LUA_FILES=`find lua_modules lua_examples -iname "*.lua"`
echo checking $LUA_FILES
./luac.cross -p $LUA_FILES
)
