#!/bin/bash

set -e

exists() {
  if command -v "$1" >/dev/null 2>&1
  then
    return 0
  else
    return 1
  fi
}

usage() {
echo "
usage: bash tools/travis/run-luacheck.sh [-s]

Avarible options are:
-s:		Standalone mode: Lua, LuaRocks and luacheck
will be installed in nodemcu-firmare/cache folder.

By default script will use luarocks installed in host system.
"
}

install_tools() {
  if ! exists luarocks; then
    echo "LuaRocks not found!"
    exit 1
  fi

  eval "`luarocks path --bin`"  #Set PATH for luacheck
  #In Travis Path it's not changed by LuaRocks for some unknown reason
  if [ "${TRAVIS}" = "true" ]; then 
  	export PATH=$PATH:/home/travis/.luarocks/bin
  fi

  if ! exists luacheck; then
  	echo "Installing luacheck"
  	luarocks install --local luacheck || exit
  fi
  
}

install_tools_standalone() {
  if ! [ -x cache/localua/bin/luarocks ]; then
  echo "Installing Lua 5.3 and LuaRocks"
  bash tools/travis/localua.sh cache/localua || exit
  fi

  if ! [ -x cache/localua/bin/luacheck ]; then
    echo "Installing luacheck"
    cache/localua/bin/luarocks install luacheck || exit
  fi
}

if [[ $1 == "" ]]; then
  	install_tools
  else
  while getopts "s" opt
  do
    case $opt in
    (s) install_tools_standalone ;;
    (*) usage; exit 1 ;;
    esac
  done
fi

echo "Static analysys of"
find lua_modules lua_examples -iname "*.lua" -print0 | xargs -0 echo

(find lua_modules lua_examples -iname "*.lua" -print0 | xargs -0 luacheck --config tools/luacheck_config.lua) || exit
