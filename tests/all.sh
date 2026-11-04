#!/usr/bin/env zsh

set -e -u -x

[ -n "${NODEMCU_WIFI:-}" ] || { echo "Specify NODEMCU_WIFI"; exit 1; }
[ -n "${NODEMCU_DUT0:-}" ] || { echo "Specify NODEMCU_DUT0"; exit 1; }

: ${NODEMCU_DUT0_NUM:=int}
: ${NODEMCU_DUT0_CONFIG:=conf/dut0/testenv.conf}

if [ -n "${NODEMCU_DUT1:-}" ]; then
  : ${NODEMCU_DUT1_NUM:=int}
  : ${NODEMCU_DUT1_CONFIG:=conf/dut1/testenv.conf}
fi

echo "Test configuration:"
echo "  DUT0 ${NODEMCU_DUT0} (${NODEMCU_DUT0_NUM} ${NODEMCU_DUT0_CONFIG})"
if [ -n "${NODEMCU_DUT1:-}" ]; then
  echo "  DUT1 ${NODEMCU_DUT1} (${NODEMCU_DUT1_NUM} ${NODEMCU_DUT1_CONFIG})"
fi

echo "Preflight..."

export TCLLIBPATH=./expectnmcu
[ -z "${NODEMCU_TESTTMP:-}" ] && {
  NODEMCU_TESTTMP="$(umask 077; mktemp -d -p /tmp nodemcu.XXXXXXXXXX)"
  cleanup() {
    rm -rf ${NODEMCU_TESTTMP}
  }
  trap cleanup EXIT
}

export NODEMCU_TESTTMP

# Bring the board up and do our IP extraction once rather than letting each
# test guess over and over.
if [ -z "${MYIP-}" ]; then
  echo "Probing for my IP address; this might take a moment..."
  MYIP=$(expect ./preflight-host-ip.expect -serial ${NODEMCU_DUT0} -wifi ${NODEMCU_WIFI})
  stty sane
  echo "Guessed my IP as ${MYIP}"
fi

./preflight-tls.sh # Make TLS keys
./preflight-lfs.sh # Make LFS images

echo "Staging LFSes and running early commands on each DUT..."

expect ./tap-driver.expect -serial ${NODEMCU_DUT0} -lfs ${NODEMCU_TESTTMP}/tmp-lfs-${NODEMCU_DUT0_NUM}.img ${NODEMCU_DUT0_CONFIG} preflight-dut.lua
if [ -n "${NODEMCU_DUT1:-}" ]; then
  expect ./tap-driver.expect -serial ${NODEMCU_DUT1} -lfs ${NODEMCU_TESTTMP}/tmp-lfs-${NODEMCU_DUT1_NUM}.img ${NODEMCU_DUT1_CONFIG} preflight-dut.lua
fi

echo "Running on-DUT tests..."

# These are all in LFS (see preflight-lfs.sh) and so we can -noxfer and just run tests by name
T=(
  NTest_adc_env
  NTest_file
  NTest_gpio_env
  NTest_pixbuf
  NTest_tmr
  NTest_ws2812_effects
  NTest_ws2812
)

for t in "${T[@]}"; do
  expect ./tap-driver.expect -serial ${NODEMCU_DUT0} -noxfer -runfunc "node.LFS.get('${t}')"
done

echo "Running from-host tests..."

T=(
  ./test-mqtt.expect
)

for t in "${T[@]}"; do
  expect ${t} -serial ${NODEMCU_DUT0} -wifi ${NODEMCU_WIFI} -ip ${MYIP}
done
