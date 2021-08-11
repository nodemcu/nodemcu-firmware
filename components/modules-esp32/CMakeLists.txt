if(NOT "${IDF_TARGET}" STREQUAL "esp32c3")

# Globbing isn't recommended, but I dislike it less than having to edit
# this file whenever a new module source file springs into existence.
# Just remember to "idf.py reconfigure" (or do a clean build) to get
# cmake to pick up on the new (or removed) files.
file(
  GLOB module_srcs
  LIST_DIRECTORIES false
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  *.c
)

idf_component_register(
  SRCS ${module_srcs}
  PRIV_INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
  PRIV_REQUIRES
    "base_nodemcu"
    "driver"
    "driver_can"
    "sdmmc"
    "esp_eth"
    "lua"
    "modules"
    "platform"
    "soc"
)

# Match up all the module source files with their corresponding Kconfig
# option in the form NODEMCU_CMODULE_<modname> and if enabled, add a
# "-u <modname>_module_selected1" option to force the linker to include
# the module. See components/core/include/module.h for further details on
# how this works.
set(modules_enabled)
foreach(module_src ${module_srcs})
  string(REPLACE ".c" "" module_name ${module_src})
  string(TOUPPER ${module_name} module_ucase)
  set(mod_opt "CONFIG_NODEMCU_CMODULE_${module_ucase}")
  if (${${mod_opt}})
    list(APPEND modules_enabled ${module_ucase})
  endif()
endforeach()
message("Including the following modules: ${modules_enabled}")

foreach(mod ${modules_enabled})
  target_link_libraries(${COMPONENT_LIB} "-u ${mod}_module_selected1")
endforeach()

endif()
