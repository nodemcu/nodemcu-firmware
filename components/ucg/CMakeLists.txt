idf_component_register(
  SRC_DIRS "ucg/src/clib"
  INCLUDE_DIRS "ucg/src/clib"
)
# 3rd-party source is not warning clean, so suppress for now.
# Revisit when upgrading the ucg submodule.
target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-unused-const-variable
  -Wno-unused-variable
  -Wno-unused-but-set-variable
  -Wno-unused-function
)
