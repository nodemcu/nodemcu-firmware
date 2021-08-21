if(CONFIG_LUA_VERSION_51)
  set(srcs
    "lua-5.1/lapi.c"
    "lua-5.1/lauxlib.c"
    "lua-5.1/lbaselib.c"
    "lua-5.1/lcode.c"
    "lua-5.1/ldblib.c"
    "lua-5.1/ldebug.c"
    "lua-5.1/ldo.c"
    "lua-5.1/ldump.c"
    "lua-5.1/lflash.c"
    "lua-5.1/lfunc.c"
    "lua-5.1/lgc.c"
    "lua-5.1/llex.c"
    "lua-5.1/lmathlib.c"
    "lua-5.1/lmem.c"
    "lua-5.1/lnodemcu.c"
    "lua-5.1/loadlib.c"
    "lua-5.1/lobject.c"
    "lua-5.1/lopcodes.c"
    "lua-5.1/lparser.c"
    "lua-5.1/lstate.c"
    "lua-5.1/lstring.c"
    "lua-5.1/lstrlib.c"
    "lua-5.1/ltable.c"
    "lua-5.1/ltablib.c"
    "lua-5.1/ltm.c"
    "lua-5.1/lua.c"
    "lua-5.1/lundump.c"
    "lua-5.1/lvm.c"
    "lua-5.1/lzio.c"
  )
  set(luadir "lua-5.1")
elseif(CONFIG_LUA_VERSION_53)
  set(srcs
    "lua-5.3/lapi.c"
    "lua-5.3/lauxlib.c"
    "lua-5.3/lbaselib.c"
    "lua-5.3/lbitlib.c"
    "lua-5.3/lcode.c"
    "lua-5.3/lcorolib.c"
    "lua-5.3/lctype.c"
    "lua-5.3/ldblib.c"
    "lua-5.3/ldebug.c"
    "lua-5.3/ldo.c"
    "lua-5.3/ldump.c"
    "lua-5.3/lfunc.c"
    "lua-5.3/lgc.c"
    "lua-5.3/llex.c"
    "lua-5.3/lmathlib.c"
    "lua-5.3/lmem.c"
    "lua-5.3/lnodemcu.c"
    "lua-5.3/loadlib.c"
    "lua-5.3/lobject.c"
    "lua-5.3/lopcodes.c"
    "lua-5.3/lparser.c"
    "lua-5.3/lstate.c"
    "lua-5.3/lstring.c"
    "lua-5.3/lstrlib.c"
    "lua-5.3/ltable.c"
    "lua-5.3/ltablib.c"
    "lua-5.3/ltm.c"
    "lua-5.3/lua.c"
    "lua-5.3/lundump.c"
    "lua-5.3/lutf8lib.c"
    "lua-5.3/lvm.c"
    "lua-5.3/lzio.c"
  )
  set(luadir "lua-5.3")
endif()

set(common_srcs
  "common/lextra.c"
  "common/lfs.c"
  "common/linput.c"
  "common/linit.c"
  "common/lnodeaux.c"
  "common/lpanic.c"
)

idf_component_register(
  SRCS ${srcs} ${common_srcs}
  INCLUDE_DIRS ${luadir} "common"
  REQUIRES "platform" "nvs_flash" "uzlib" "driver_console"
  PRIV_REQUIRES "base_nodemcu" "embedded_lfs"
  LDFRAGMENTS "link_lua.lf"
)

#
# Cross compiler targets
#

# Not sure why we can't directly depend on ${SDKCONFIG_HEADER} in our
# externalproject_add(), but them's the brakes...

set(luac_build_dir "${BUILD_DIR}/luac_cross")

add_custom_command(
  OUTPUT sdkconfig.h
  COMMAND mkdir -p "${luac_build_dir}" && cp ${SDKCONFIG_HEADER} ${luac_build_dir}/sdkconfig.h
  DEPENDS ${SDKCONFIG_HEADER}
  VERBATIM
)
add_custom_target(sdkconfig_h DEPENDS sdkconfig.h)

externalproject_add(luac_cross_build
  PREFIX ${luac_build_dir}
  SOURCE_DIR ${COMPONENT_DIR}/${luadir}/host
  CONFIGURE_COMMAND ""
  BUILD_COMMAND make -f ${COMPONENT_DIR}/${luadir}/host/idf.mk LUAC_BUILD_DIR=${luac_build_dir} LUA_PATH=${COMPONENT_DIR}/${luadir} PYTHON=${PYTHON}
  INSTALL_COMMAND ""
  BUILD_ALWAYS 1
  DEPENDS sdkconfig_h
)

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
  "${BUILD_DIR}/luac_cross/luac.cross;${BUILD_DIR}/luac_cross/${luadir}" )
