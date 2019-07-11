#!/usr/bin/env bash

USER_MODULES_H=app/include/user_modules.h
USER_CONFIG_H=app/include/user_config.h

BUILD_DATE="$(date "+%Y-%m-%d %H:%M")"
COMMIT_ID="$(git rev-parse HEAD)"
BRANCH="$(git rev-parse --abbrev-ref HEAD | sed -r 's/[\/\\]+/_/g')"
RELEASE="$(git describe --tags --long | sed -r 's/(.*)-(.*)-.*/\1 +\2/g' | sed 's/ +0$//')"
RELEASE_DTS=0$(git show -s --format=%cd --date=format:"%Y%m%d%H%M" HEAD)

# figure out whether SSL is enabled in user_config.h
if grep -Eq "^#define CLIENT_SSL_ENABLE" $USER_CONFIG_H; then
    SSL="true"
  else
    SSL="false"
  fi

# figure out whether LFS configuration in user_config.h
LFS=$(grep "^#define LUA_FLASH_STORE" $USER_CONFIG_H | tr -d '\r' | cut -d ' ' -f 3-)
if [ -z "$LFS" ]; then
    LFS="disabled"
else
    LFS="Size: ${LFS}"
  fi

# figure out whether Int build is enabled in user_config.h
if grep -Eq "^#define LUA_NUMBER_INTEGRAL" $USER_CONFIG_H; then
  BUILD_TYPE=integer
else
  BUILD_TYPE=float
fi

MODULES=$(awk '/^[ \t]*#define LUA_USE_MODULES/{modules=modules sep tolower(substr($2,17));sep=","}END{if(length(modules)==0)modules="-";print modules}' $USER_MODULES_H | tr -d '\r')

# create temp buildinfo
TEMPFILE=/tmp/buildinfo.h
cat > $TEMPFILE << EndOfMessage
#ifndef __BUILDINFO_H__
#define __BUILDINFO_H__
EndOfMessage

echo "#define BUILDINFO_BRANCH \""$BRANCH"\"" >> $TEMPFILE
echo "#define BUILDINFO_COMMIT_ID \""$COMMIT_ID"\"" >> $TEMPFILE
echo "#define BUILDINFO_RELEASE \""$RELEASE"\"" >> $TEMPFILE
echo "#define BUILDINFO_RELEASE_DTS "$RELEASE_DTS >> $TEMPFILE
echo "#define BUILDINFO_RELEASE_DTS_STR \""$RELEASE_DTS"\"" >> $TEMPFILE
echo "#define BUILDINFO_SSL "$SSL >> $TEMPFILE
echo "#define BUILDINFO_SSL_STR \""$SSL"\"" >> $TEMPFILE
echo "#define BUILDINFO_BUILD_TYPE \""$BUILD_TYPE"\"" >> $TEMPFILE
echo "#define BUILDINFO_LFS \""$LFS"\"" >> $TEMPFILE
echo "#define BUILDINFO_MODULES \""$MODULES"\"" >> $TEMPFILE

cat >> $TEMPFILE << EndOfMessage2

#define NODE_VERSION_LONG \\
	"\tbranch: '" BUILDINFO_BRANCH "'\n" \\
	"\tcommit: '" BUILDINFO_COMMIT_ID "'\n" \\
	"\trelease: '" BUILDINFO_RELEASE "'\n" \\
	"\trelease DTS: '" BUILDINFO_RELEASE_DTS_STR "'\n" \\
	"\tSSL: '" BUILDINFO_SSL_STR "'\n" \\
	"\tBuild type: '" BUILDINFO_BUILD_TYPE "'\n" \\
	"\tLFS: '" BUILDINFO_LFS "'\n" \\
	"\tmodules: '" BUILDINFO_MODULES "'\n"
	
EndOfMessage2

echo "#endif	/* __BUILDINFO_H__ */" >> $TEMPFILE

diff -q $TEMPFILE app/include/buildinfo.h || cp $TEMPFILE app/include/buildinfo.h
rm $TEMPFILE