# Modules common to all chips
set(module_srcs
  "adc.c"
  "bit.c"
  "bthci.c"
  "common.c"
  "crypto.c"
  "dht.c"
  "encoder.c"
  "file.c"
  "gpio.c"
  "heaptrace.c"
  "http.c"
  "httpd.c"
  "i2c.c"
  "i2c_hw_master.c"
  "i2c_hw_slave.c"
  "ledc.c"
  "mqtt.c"
  "net.c"
  "node.c"
  "otaupgrade.c"
  "ow.c"
  "pipe.c"
  "qrcodegen.c"
  "sigma_delta.c"
  "sjson.c"
  "sodium.c"
  "spi.c"
  "spi_master.c"
  "struct.c"
  "time.c"
  "tmr.c"
  "u8g2.c"
  "uart.c"
  "ucg.c"
  "wifi.c"
  "wifi_ap.c"
  "wifi_common.c"
  "wifi_sta.c"
  "ws2812.c"
)

# Chip specific modules, per module.
# List source files for each applicable chip.
if(IDF_TARGET STREQUAL "esp32")
  list(APPEND module_srcs
    "can.c"
    "dac.c"
    "eth.c"
    "i2s.c"
    "pulsecnt.c"
    "sdmmc.c"
    "touch.c"
  )
elseif(IDF_TARGET STREQUAL "esp32s2")
  list(APPEND module_srcs
    "dac.c"
  )
elseif(IDF_TARGET STREQUAL "esp32s3")
  list(APPEND module_srcs
  )
elseif(IDF_TARGET STREQUAL "esp32c3")
  list(APPEND module_srcs
  )
endif()

idf_component_register(
  SRCS ${module_srcs}
  INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
  PRIV_REQUIRES
    "app_update"
    "base_nodemcu"
    "bt"
    "driver_can"
    "esp_http_client"
    "fatfs"
    "esp_http_server"
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
