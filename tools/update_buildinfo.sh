#!/usr/bin/env bash

USER_MODULES_H=app/include/user_modules.h
USER_CONFIG_H=app/include/user_config.h

BUILD_DATE="$(date "+%Y-%m-%d %H:%M")"
COMMIT_ID="$(git rev-parse HEAD)"
BRANCH="$(git rev-parse --abbrev-ref HEAD | sed -r 's/[\/\\]+/_/g')"
RELEASE="$(git describe --tags --long | sed -r 's/(.*)-(.*)-.*/\1 +\2/g' | sed 's/ +0$//')"
RELEASE_DTS=$(TZ=UTC git show --quiet --date=format-local:"%Y%m%d%H%M" --format="%cd" HEAD)

MODULES=$(awk '/^[ \t]*#define LUA_USE_MODULES/{modules=modules sep tolower(substr($2,17));sep=","}END{if(length(modules)==0)modules="-";print modules}' $USER_MODULES_H | tr -d '\r')

# create temp buildinfo
TEMPFILE=/tmp/buildinfo.h
cat > $TEMPFILE << EndOfMessage
#ifndef __BUILDINFO_H__
#define __BUILDINFO_H__

#include "user_config.h"

#define BUILDINFO_STR_HELPER(x) #x
#define BUILDINFO_TO_STR(x)	BUILDINFO_STR_HELPER(x)

#ifdef LUA_FLASH_STORE
#define BUILDINFO_LFS LUA_FLASH_STORE
#else
#define BUILDINFO_LFS 0
#endif

#ifdef CLIENT_SSL_ENABLE
#define BUILDINFO_SSL true
#define BUILDINFO_SSL_STR "true"
#else
#define BUILDINFO_SSL false
#define BUILDINFO_SSL_STR "false"
#endif

#ifdef LUA_NUMBER_INTEGRAL
#define BUILDINFO_BUILD_TYPE "integer"
#else
#define BUILDINFO_BUILD_TYPE "float"
#endif

EndOfMessage

echo "#define USER_PROLOG \""$USER_PROLOG"\"" >> $TEMPFILE
echo "#define BUILDINFO_BRANCH \""$BRANCH"\"" >> $TEMPFILE
echo "#define BUILDINFO_COMMIT_ID \""$COMMIT_ID"\"" >> $TEMPFILE
echo "#define BUILDINFO_RELEASE \""$RELEASE"\"" >> $TEMPFILE
echo "#define BUILDINFO_RELEASE_DTS \""$RELEASE_DTS"\"" >> $TEMPFILE
echo "#define BUILDINFO_MODULES \""$MODULES"\"" >> $TEMPFILE

cat >> $TEMPFILE << EndOfMessage2
#define NODE_VERSION_LONG \\
  "$USER_PROLOG \n" \\
  "\tbranch: " BUILDINFO_BRANCH "\n" \\
  "\tcommit: " BUILDINFO_COMMIT_ID "\n" \\
  "\trelease: " BUILDINFO_RELEASE "\n" \\
  "\trelease DTS: " BUILDINFO_RELEASE_DTS "\n" \\
  "\tSSL: " BUILDINFO_SSL_STR "\n" \\
  "\tBuild type: " BUILDINFO_BUILD_TYPE "\n" \\
  "\tLFS: " BUILDINFO_TO_STR(BUILDINFO_LFS) "\n" \\
  "\tmodules: " BUILDINFO_MODULES "\n"

EndOfMessage2

echo "#endif	/* __BUILDINFO_H__ */" >> $TEMPFILE

diff -q $TEMPFILE app/include/buildinfo.h || cp $TEMPFILE app/include/buildinfo.h
rm $TEMPFILE
