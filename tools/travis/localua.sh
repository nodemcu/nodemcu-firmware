#!/bin/bash

# Downloads and installs a self-contained Lua and LuaRocks.
# Supports Linux, macOS and MSYS2.
# Copyright (c) 2015-2019 Pierre Chapuis, MIT Licensed.
# Latest stable version available at: https://loadk.com/localua.sh
# Maintained at: https://github.com/oploadk/localua

DEFAULT_LUA_V="5.3.5"
DEFAULT_LR_V="3.0.4"

usage () {
    >&2 echo -e "USAGE: $0 output-dir [lua-version] [luarocks-version]\n"
    >&2 echo -n "The first optional argument is the Lua version, "
    >&2 echo -n "the second one is the LuaRocks version. "
    >&2 echo -e "Defaults are Lua $DEFAULT_LUA_V and LuaRocks $DEFAULT_LR_V.\n"
    >&2 echo -n "You can set a custom build directory with environment "
    >&2 echo -e "variable LOCALUA_BUILD_DIRECTORY (not useful in general).\n"
    >&2 echo -e "You can set a custom makefile target with LOCALUA_TARGET.\n"
    >&2 echo -e "You can disable LUA_COMPAT by setting LOCALUA_NO_COMPAT.\n"
    >&2 echo -e "You can skip luarocks by setting LOCALUA_NO_LUAROCKS."
    exit 1
}

# Set output directory, Lua version and LuaRocks version

ODIR="$1"
[ -z "$ODIR" ] && usage

LUA_V="$2"
[ -z "$LUA_V" ] && LUA_V="$DEFAULT_LUA_V"

LUA_SHORTV="$(echo $LUA_V | cut -c 1-3)"

LR_V="$3"
[ -z "$LR_V" ] && LR_V="$DEFAULT_LR_V"

# Set build directory

BDIR="$LOCALUA_BUILD_DIRECTORY"
[ -z "$BDIR" ] && BDIR="$(mktemp -d /tmp/localua-XXXXXX)"

# Create output directory and get absolute path

mkdir -p "$ODIR"
>/dev/null pushd "$ODIR"
    ODIR="$(pwd)"
>/dev/null popd

# Download, unpack and build Lua and LuaRocks

if [ -z "$LOCALUA_TARGET" ]; then
    case "$(uname)" in
        Linux)
            LOCALUA_TARGET="linux";;
        Darwin)
            LOCALUA_TARGET="macosx";;
        MSYS*)
            LOCALUA_TARGET="msys";;
        *)
            LOCALUA_TARGET="posix";;
    esac
fi

pushd "$BDIR"
    wget "http://www.lua.org/ftp/lua-${LUA_V}.tar.gz"
    tar xf "lua-${LUA_V}.tar.gz"
    pushd "lua-${LUA_V}"
        sed 's#"/usr/local/"#"'"$ODIR"'/"#' "src/luaconf.h" > "$BDIR/t"
        mv "$BDIR/t" "src/luaconf.h"
        if [ ! -z "$LOCALUA_NO_COMPAT" ]; then
            sed 's#-DLUA_COMPAT_5_2##' "src/Makefile" > "$BDIR/t"
            sed 's#-DLUA_COMPAT_ALL##' "$BDIR/t" > "src/Makefile"
        fi

        if [ "$LOCALUA_TARGET" = "msys" ]; then
            >> "src/Makefile" echo
            >> "src/Makefile" echo 'msys:' >> "src/Makefile"
            >> "src/Makefile" echo -ne "\t"
            >> "src/Makefile" echo '$(MAKE) "LUA_A=lua53.dll" "LUA_T=lua.exe" \'
            >> "src/Makefile" echo -ne "\t"
            >> "src/Makefile" echo '"AR=$(CC) -shared -Wl,--out-implib,liblua.dll.a -o" "RANLIB=strip --strip-unneeded" \'
            >> "src/Makefile" echo -ne "\t"
            >> "src/Makefile" echo '"SYSCFLAGS=-DLUA_BUILD_AS_DLL -DLUA_USE_POSIX -DLUA_USE_DLOPEN" "SYSLIBS=" "SYSLDFLAGS=-s" lua.exe'
            >> "src/Makefile" echo -ne "\t"
            >> "src/Makefile" echo '$(MAKE) "LUAC_T=luac.exe" luac.exe'

            make -C src "$LOCALUA_TARGET" || exit 1
            make \
                TO_BIN="lua.exe luac.exe lua53.dll" \
                TO_LIB="liblua.a liblua.dll.a" \
                INSTALL_TOP="$ODIR" install || exit 1
        else
            make "$LOCALUA_TARGET" || exit 1
            make INSTALL_TOP="$ODIR" install || exit 1
        fi

    popd
    if [ -z "$LOCALUA_NO_LUAROCKS" ]; then
        wget "http://luarocks.org/releases/luarocks-${LR_V}.tar.gz"
        tar xf "luarocks-${LR_V}.tar.gz"
        pushd "luarocks-${LR_V}"
            ./configure --with-lua="$ODIR" --prefix="$ODIR" \
                        --lua-version="$LUA_SHORTV" \
                        --sysconfdir="$ODIR/luarocks" --force-config
            make bootstrap
        popd
    fi
popd

# Cleanup

rm -rf "$BDIR"
