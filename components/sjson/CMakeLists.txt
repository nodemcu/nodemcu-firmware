idf_component_register(
  SRCS "jsonsl.c"
  INCLUDE_DIRS "."
  REQUIRES "platform"
)
target_compile_options(${COMPONENT_LIB} PRIVATE
  -imacros json_config.h
)
