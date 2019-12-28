#!/bin/bash

set -e

#Download luacheck binary if nessesary
if ! [ -x "cache/luacheck.exe" ]; then
    wget -O cache/luacheck.exe https://github.com/mpeterv/luacheck/releases/download/0.23.0/luacheck.exe
fi

echo "Static analysys of"
find lua_modules lua_examples -iname "*.lua" -print0 | xargs -0 echo

(find lua_modules lua_examples -iname "*.lua" -print0 | xargs -0 cache/luacheck.exe --config tools/luacheck_config.lua) || exit
