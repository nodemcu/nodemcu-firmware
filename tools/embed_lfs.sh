#!/bin/bash

LUA_APP_SRC=("$@")

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

# Extract the start/end symbols of the LFS object, then calculate the
# available size from that.
LFS_ADDR=$(grep -E '0x[0-9a-f]+ +_binary_lua_flash_store_reserved_start' "${MAP_FILE}" | awk '{print $1}')
LFS_ADDR_END=$(grep -E '0x[0-9a-f]+ +_binary_lua_flash_store_reserved_end' "${MAP_FILE}" | awk '{print $1}')
if [ "${LFS_ADDR}" = "" ]
then
  echo "Error: LFS segment address not found"
  exit 1
fi
LFS_SIZE=$((LFS_ADDR_END - LFS_ADDR))

printf "LFS segment address %s, length %s (0x%x)\n" "${LFS_ADDR}" "${LFS_SIZE}" "${LFS_SIZE}"

if ${LUAC_CROSS} -v | grep -q 'Lua 5.1'
then
  echo "Generating Lua 5.1 LFS image..."
  ${LUAC_CROSS} -a "${LFS_ADDR}" -m ${LFS_SIZE} -o ${LUAC_OUTPUT} "${LUA_APP_SRC[@]}"
else
  set -e
  echo "Generating intermediate Lua 5.3 LFS image..."
  ${LUAC_CROSS} -f -m ${LFS_SIZE} -o ${LUAC_OUTPUT}.tmp "${LUA_APP_SRC[@]}"
  echo "Converting to absolute LFS image..."
  ${LUAC_CROSS} -F ${LUAC_OUTPUT}.tmp -a "${LFS_ADDR}" -o ${LUAC_OUTPUT}
  rm ${LUAC_OUTPUT}.tmp
fi
# shellcheck disable=SC2181
if [ $? != 0 ]; then
	echo "Error: luac.cross failed"
	exit 1
else
  echo "Generated $(stat -c "%s" ${LUAC_OUTPUT}) bytes of LFS data"
fi
# cmake depencies don't seem to pick up the change to luac.out?
rm -f build/lua.flash.store.reserved

echo "Re-linking nodemcu binary by invoking IDF build..."
make >/dev/null
echo "Done."
