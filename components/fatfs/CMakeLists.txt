idf_component_register(
  SRCS "diskio.c" "ff.c" "myfatfs.c" "ffunicode.c" "ffsystem.c"
  INCLUDE_DIRS "."
  REQUIRES "platform" "sdmmc"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -imacros fatfs_prefix_lib.h)

# TODO: add Kconfig to select code page used?
