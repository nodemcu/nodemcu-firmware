#!/bin/sh

set -e

echo "Running PR build (all modules, SSL enabled, debug enabled)"
(
cd "$TRAVIS_BUILD_DIR"/app/include || exit
# uncomment disabled modules e.g. '//#define LUA_USE_MODULES_UCG' -> '#define LUA_USE_MODULES_UCG'
sed -i -r 's@(//.*)(#define *LUA_USE_MODULES_.*)@\2@g' user_modules.h
cat user_modules.h

# enable SSL
sed -i 's@//#define CLIENT_SSL_ENABLE@#define CLIENT_SSL_ENABLE@' user_config.h

# enable debug
sed -i 's@// ?#define DEVELOP_VERSION@#define DEVELOP_VERSION@' user_config.h

# enable FATFS
sed -i 's@//#define BUILD_FATFS@#define BUILD_FATFS@' user_config.h

# enable new I2C driver
sed -i 's@#define I2C_MASTER_OLD_VERSION@//#define I2C_MASTER_OLD_VERSION@' user_config.h

# enable pmSleep
sed -i 's@//#define TIMER_SUSPEND_ENABLE@#define TIMER_SUSPEND_ENABLE@' user_config.h
sed -i 's@//#define PMSLEEP_ENABLE@#define PMSLEEP_ENABLE@' user_config.h

# enable WiFi SmartConfig
sed -i 's@//#define WIFI_SMART_ENABLE@#define WIFI_SMART_ENABLE@' user_config.h

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
)
