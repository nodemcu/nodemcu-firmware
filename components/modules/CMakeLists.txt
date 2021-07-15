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
    "app_update"
    "base_nodemcu"
    "bt"
    "driver_can"
    "esp_http_client"
    "libsodium"
    "lua"
    "mbedtls"
    "mqtt"
    "platform"
    "qrcodegen"
    "sdmmc"
    "sjson"
    "soc"
    "u8g2"
    "ucg"
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
  #message("checking ${mod_opt}")
  if (${${mod_opt}})
    list(APPEND modules_enabled ${module_ucase})
  endif()
endforeach()
message("Including the following modules: ${modules_enabled}")

foreach(mod ${modules_enabled})
  target_link_libraries(${COMPONENT_LIB} "-u ${mod}_module_selected1")
endforeach()


# Auto-generation of ucg/u8g2 header files, per
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#source-code-generation

add_custom_command(
  OUTPUT ucg_config.h
  COMMAND perl -w ${PROJECT_DIR}/tools/ucg_config.pl < ${SDKCONFIG_HEADER} > ucg_config.h
  DEPENDS ${SDKCONFIG_HEADER}
  VERBATIM
)
add_custom_target(ucg_config DEPENDS ucg_config.h)

add_custom_command(
  OUTPUT u8g2_fonts.h
  COMMAND perl -w ${PROJECT_DIR}/tools/u8g2_config_fonts.pl < ${SDKCONFIG_HEADER} > u8g2_fonts.h
  DEPENDS ${SDKCONFIG_HEADER}
  VERBATIM
)
add_custom_target(u8g2_fonts DEPENDS u8g2_fonts.h)
add_custom_command(
  OUTPUT u8g2_displays.h
  COMMAND perl -w ${PROJECT_DIR}/tools/u8g2_config_displays.pl < ${SDKCONFIG_HEADER} > u8g2_displays.h
)
add_custom_target(u8g2_displays DEPENDS u8g2_displays.h)

add_dependencies(${COMPONENT_LIB} ucg_config u8g2_fonts u8g2_displays)

set_property(
  DIRECTORY "${COMPONENT_DIR}" APPEND
  PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ucg_config.h u8g2_fonts.h u8g2_displays.h
)
