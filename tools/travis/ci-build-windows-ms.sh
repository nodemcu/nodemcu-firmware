#!/bin/sh

set -e

echo "Running ci build for windows msbuild (supports only hosttools)"
(
  cd "$TRAVIS_BUILD_DIR"/msvc || exit
  export PATH=$MSBUILD_PATH:$PATH
  msbuild.exe hosttools.sln
)

