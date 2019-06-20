#!/bin/bash

LUA_APP_SRC="$@"

MAP_FILE=build/NodeMCU.map
LUAC_OUTPUT=build/luac.out
LUAC_CROSS=build/luac_cross/luac.cross

if [ ! -f "${MAP_FILE}" ]; then
	echo "Error: ${MAP_FILE} not found. Please run make first."
	exit 1
fi
if [ ! -f "${LUAC_CROSS}" ]; then
	echo "Error: ${LUAC_CROSS} not found. Please run make first."
	exit 1
fi

LFS_ADDR_SIZE=`grep -E "\.lfs\.reserved[ ]+0x[0-9a-f]+[ ]+0x[0-9a-z]+" ${MAP_FILE} | tr -s ' '`
if [ -z "${LFS_ADDR_SIZE}" ]; then
	echo "Error: LFS segment not found. Use 'make clean; make' perhaps?"
	exit 1
fi

LFS_ADDR=`echo "${LFS_ADDR_SIZE}" | cut -d ' ' -f 3`
if [ -z "${LFS_ADDR}" ]; then
	echo "Error: LFS segment address not found"
	exit 1
fi
LFS_SIZE=`echo "${LFS_ADDR_SIZE}" | cut -d ' ' -f 4`
if [ -z "${LFS_SIZE}" ]; then
	echo "Error: LFS segment size not found"
	exit 1
fi

echo "LFS segment address ${LFS_ADDR}, length ${LFS_SIZE}"

${LUAC_CROSS} -a ${LFS_ADDR} -m ${LFS_SIZE} -o ${LUAC_OUTPUT} ${LUA_APP_SRC}
if [ $? != 0 ]; then
	echo "Error: luac.cross failed"
	exit 1
fi

make
