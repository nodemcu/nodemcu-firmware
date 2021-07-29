idf_component_register(
  SRC_DIRS "." "u8g2/src/clib"
  INCLUDE_DIRS "u8g2/src/clib"
  PRIV_REQUIRES "lua" "platform"
)
# 3rd-party source is not warning clean, so suppress for now.
# Revisit when upgrading the ucg submodule.
target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-unused-const-variable
)
