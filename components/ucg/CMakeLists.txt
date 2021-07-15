idf_component_register(
  SRC_DIRS "ucg/src/clib"
  INCLUDE_DIRS "ucg/src/clib"
)
target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-error=unused-const-variable
)
