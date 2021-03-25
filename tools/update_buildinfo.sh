#!/usr/bin/env bash

COMMIT_ID="$(git rev-parse HEAD)"
BRANCH="$(git rev-parse --abbrev-ref HEAD | sed -E 's/[\/\\]+/_/g')"
RELEASE="$(git describe --tags --long | sed -E 's/(.*)-(.*)-.*/\1 +\2/g' | sed 's/ +0$//')"
RELEASE_DTS=$(TZ=UTC git show --quiet --date=format-local:"%Y%m%d%H%M" --format="%cd" HEAD)
BUILD_DATE="$(date "+%Y-%m-%d %H:%M")"

# create temp buildinfo
TEMPFILE=/tmp/buildinfo.h
cat > $TEMPFILE << EndOfMessage
#ifndef __BUILDINFO_H__
#define __BUILDINFO_H__

#define USER_PROLOG "$USER_PROLOG"
#define BUILDINFO_BRANCH "$BRANCH"
#define BUILDINFO_COMMIT_ID "$COMMIT_ID"
#define BUILDINFO_RELEASE "$RELEASE"
#define BUILDINFO_RELEASE_DTS "$RELEASE_DTS"
#define BUILDINFO_BUILD_DATE "$BUILD_DATE"

EndOfMessage

echo "#endif	/* __BUILDINFO_H__ */" >> $TEMPFILE

diff -q $TEMPFILE components/platform/include/buildinfo.h || cp $TEMPFILE components/platform/include/buildinfo.h
rm $TEMPFILE
