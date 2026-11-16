#!/usr/bin/env zsh

set -e -u

SELFDIR=$(dirname $(readlink -f $0))

LUAFILES=(
  # Infrastructure
  ${SELFDIR}/../lua_examples/lfs/_init.lua
  ${SELFDIR}/../lua_examples/lfs/dummy_strings.lua
  ${SELFDIR}/../lua_examples/pipeutils.lua # accelerates TCL xfer

  ${SELFDIR}/NTest/NTest.lua
  ${SELFDIR}/utils/NTestEnv.lua
  ${SELFDIR}/utils/NTestTapOut.lua

  # Lua modules exercised by test programs

  # Test programs
  ${SELFDIR}/NTest_*.lua
)

if [ -e ${SELFDIR}/../luac.cross.int ]; then
  echo "Found integer Lua cross compiler..."
  ${SELFDIR}/../luac.cross.int -f -o ${NODEMCU_TESTTMP}/tmp-lfs-int.img   ${LUAFILES[@]}
  echo " ... and generated ${NODEMCU_TESTTMP}/tmp-lfs-int.img"
fi

if [ -e ${SELFDIR}/../luac.cross ]; then 
  echo "Found float Lua cross compiler..."
  ${SELFDIR}/../luac.cross     -f -o ${NODEMCU_TESTTMP}/tmp-lfs-float.img ${LUAFILES[@]}
  echo " ... and generated ${NODEMCU_TESTTMP}/tmp-lfs-float.img"
fi
