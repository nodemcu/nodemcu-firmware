idf_component_register(
  SRC_DIRS "." "u8g2/src/clib"
  INCLUDE_DIRS "u8g2/src/clib"
  PRIV_REQUIRES "lua" "platform"
)
target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-error=unused-const-variable
)
