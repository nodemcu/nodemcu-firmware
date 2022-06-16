#!/bin/bash

LUA_APP_SRC="$@"

MAP_FILE=build/nodemcu.map
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

LFS_ADDR_SIZE=$(grep -E "0x[0-9a-f]+[ ]+0x[0-9a-f]+[ ]+esp-idf/embedded_lfs/libembedded_lfs.a\(lua.flash.store.reserved.S.obj\)" "${MAP_FILE}" | grep -v -w 0x0 | grep -v -w 0x24 | tr -s ' ')
if [ -z "${LFS_ADDR_SIZE}" ]; then
	echo "Error: LFS segment not found. Use 'make clean; make' perhaps?"
	exit 1
fi

LFS_ADDR=$(echo "${LFS_ADDR_SIZE}" | cut -d ' ' -f 2)
if [ -z "${LFS_ADDR}" ]; then
	echo "Error: LFS segment address not found"
	exit 1
fi
# The reported size is +4 due to the length field added by the IDF
LFS_SIZE=$(( $(echo "${LFS_ADDR_SIZE}" | cut -d ' ' -f 3) - 4 ))
if [ -z "${LFS_SIZE}" ]; then
	echo "Error: LFS segment size not found"
	exit 1
fi

echo "LFS segment address ${LFS_ADDR}, length ${LFS_SIZE}"

if ${LUAC_CROSS} -v | grep -q 'Lua 5.1'
then
  echo "Generating Lua 5.1 LFS image..."
  ${LUAC_CROSS} -a ${LFS_ADDR} -m ${LFS_SIZE} -o ${LUAC_OUTPUT} ${LUA_APP_SRC}
else
  set -e
  echo "Generating intermediate Lua 5.3 LFS image..."
  ${LUAC_CROSS} -f -m ${LFS_SIZE} -o ${LUAC_OUTPUT}.tmp ${LUA_APP_SRC}
  echo "Converting to absolute LFS image..."
  ${LUAC_CROSS} -F ${LUAC_OUTPUT}.tmp -a ${LFS_ADDR} -o ${LUAC_OUTPUT}
  rm ${LUAC_OUTPUT}.tmp
fi
if [ $? != 0 ]; then
	echo "Error: luac.cross failed"
	exit 1
else
  echo "Generated $(ls -l ${LUAC_OUTPUT} | cut -f5 -d' ') bytes of LFS data"
fi
# cmake depencies don't seem to pick up the change to luac.out?
rm -f build/lua.flash.store.reserved

echo "Re-linking nodemcu binary by invoking IDF build..."
make >/dev/null
echo "Done."
