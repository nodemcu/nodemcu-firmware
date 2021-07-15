if (CONFIG_NODEMCU_EMBED_LFS)

# Handle hex size in a shell agnostic manner
math(EXPR lfs_size ${CONFIG_NODEMCU_EMBEDDED_LFS_SIZE})

# The luac.out gets written by tools/embed_lfs.sh, but we need a non-empty
# file for the first build pass
add_custom_command(
  OUTPUT ${BUILD_DIR}/luac.out
  COMMAND [ -s "${BUILD_DIR}/luac.out" ] || echo "" > "${BUILD_DIR}/luac.out"
  VERBATIM
)
add_custom_target(luac_out DEPENDS ${BUILD_DIR}/luac.out)

# We use the automatic file name to symbol generation here, to create the
# expected symbol lua_flash_store_reserved
add_custom_command(
  OUTPUT ${BUILD_DIR}/lua.flash.store.reserved
  COMMAND truncate --size=${lfs_size} ${BUILD_DIR}/lua.flash.store.reserved && dd if=${BUILD_DIR}/luac.out of=${BUILD_DIR}/lua.flash.store.reserved conv=notrunc status=none
  DEPENDS luac_out
  VERBATIM
)
add_custom_target(reserved_lfs DEPENDS ${BUILD_DIR}/lua.flash.store.reserved)

idf_component_register(
  EMBED_FILES ${BUILD_DIR}/lua.flash.store.reserved
  LDFRAGMENTS "embedded_lfs.lf"
)

add_dependencies(${COMPONENT_LIB} reserved_lfs)

else()
idf_component_register()
endif()
