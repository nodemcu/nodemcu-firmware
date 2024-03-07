#!/bin/sh

SELFDIR=$(dirname $(readlink -f $0))
cd ${NODEMCU_TESTTMP}

exec /usr/sbin/mosquitto -c ${SELFDIR}/test-mqtt.mosquitto.conf
