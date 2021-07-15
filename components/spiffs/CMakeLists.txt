idf_component_register(
  SRCS "spiffs.c" "spiffs_cache.c" "spiffs_check.c" "spiffs_gc.c"
    "spiffs_hydrogen.c" "spiffs_nucleus.c"
  INCLUDE_DIRS "."
  REQUIRES "spi_flash"
  PRIV_REQUIRES "platform"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=pointer-sign )
