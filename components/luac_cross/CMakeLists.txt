idf_component_register(luac_cross)

# Not sure why we can't directly depend on ${SDKCONFIG_HEADER} in our
# externalproject_add(), but them's the brakes...

add_custom_command(
  OUTPUT sdkconfig.h
  COMMAND cp ${SDKCONFIG_HEADER} sdkconfig.h
  DEPENDS ${SDKCONFIG_HEADER}
  VERBATIM
)
add_custom_target(sdkconfig_h DEPENDS sdkconfig.h)

externalproject_add(luac_cross_build
  PREFIX ${BUILD_DIR}/luac_cross
  SOURCE_DIR ${COMPONENT_DIR}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND make -f ${COMPONENT_DIR}/Makefile BUILD_DIR_BASE=${BUILD_DIR} COMPONENT_PATH=${COMPONENT_DIR} CONFIG_LUA_OPTIMIZE_DEBUG=${CONFIG_LUA_OPTIMIZE_DEBUG} PYTHON=${PYTHON}
  INSTALL_COMMAND ""
  BUILD_ALWAYS 1
  DEPENDS sdkconfig_h
)
