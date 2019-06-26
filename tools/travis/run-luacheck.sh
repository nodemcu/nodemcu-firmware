#!/bin/bash

set -e

echo "Installing Lua 5.3, LuaRocks and Luacheck"
(
  cd "$TRAVIS_BUILD_DIR" || exit
  bash tools/travis/localua.sh cache/localua || exit
  cache/localua/bin/luarocks install luacheck || exit
)

(
  echo "Static analysys of:"
  find lua_modules lua_examples -iname "*.lua" -print0 | xargs -0 echo
)

(find lua_modules lua_examples -iname "*.lua" -print0 | xargs -0 cache/localua/bin/luacheck --config tools/luacheck_config.lua) || exit
