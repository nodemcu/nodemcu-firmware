# Modules common to all chips
set(wifi_modules
  "espnow.c"
  "wifi.c"
  "wifi_ap.c"
  "wifi_common.c"
  "wifi_sta.c"
)
set(module_srcs
  "adc.c"
  "bit.c"
  "bthci.c"
  "common.c"
  "crypto.c"
  "dht.c"
  "encoder.c"
  "eromfs.c"
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
  "rotary_driver.c"
  "rotary.c"
  "rmt.c"
  "rtcmem.c"
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
    ${wifi_modules}
  )
elseif(IDF_TARGET STREQUAL "esp32s2")
  list(APPEND module_srcs
    "dac.c"
    "pulsecnt.c"
    ${wifi_modules}
  )
elseif(IDF_TARGET STREQUAL "esp32s3")
  list(APPEND module_srcs
    "dac.c"
    "pulsecnt.c"
    "sdmmc.c"
    ${wifi_modules}
  )
elseif(IDF_TARGET STREQUAL "esp32c3")
  list(APPEND module_srcs
    ${wifi_modules}
  )
elseif(IDF_TARGET STREQUAL "esp32c6")
  list(APPEND module_srcs
    "dac.c"
    "pulsecnt.c"
    ${wifi_modules}
  )
elseif(IDF_TARGET STREQUAL "esp32h2")
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
    "esp_eth"
    "esp_http_client"
    "esp_http_server"
    "esp_hw_support"
    "fatfs"
    "libsodium"
    "lua"
    "mbedtls"
    "mqtt"
    "platform"
    "qrcodegen"
    "sdmmc"
    "spi_flash"
    "sjson"
    "soc"
    "u8g2"
    "ucg"
    "vfs"
)

# Match up all the module source files with their corresponding Kconfig
# option in the form NODEMCU_CMODULE_<modname> and if enabled, add a
# "-u <modname>_module_selected1" option to force the linker to include
# the module. See components/core/include/module.h for further details on
# how this works.
set(buildinfo_modules)
set(modules_enabled)
foreach(module_src ${module_srcs})
  string(REPLACE ".c" "" module_name ${module_src})
  string(TOUPPER ${module_name} module_ucase)
  set(mod_opt "CONFIG_NODEMCU_CMODULE_${module_ucase}")
  if (${${mod_opt}})
    list(APPEND modules_enabled ${module_ucase})
    list(APPEND buildinfo_modules ${module_name})
  endif()
endforeach()
message("Including the following modules: ${modules_enabled}")
string(REPLACE ";" "," buildinfo_modules "${buildinfo_modules}")

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

# eromfs generation
add_custom_command(
  OUTPUT eromfs.bin
  COMMAND ${COMPONENT_DIR}/eromfs.py ${CONFIG_NODEMCU_CMODULE_EROMFS_VOLUMES}
  DEPENDS ${SDKCONFIG_HEADER}
)
add_custom_target(eromfs_bin DEPENDS eromfs.bin)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_BINARY_DIR}/eromfs.bin" BINARY DEPENDS eromfs_bin)
set_property(
  DIRECTORY "${COMPONENT_DIR}" APPEND
  PROPERTY ADDITIONAL_MAKE_CLEAN_FILES eromfs.bin
)


set(buildinfo_h
"#ifndef __BUILDINFO_H__
#define __BUILDINFO_H__

#ifdef CONFIG_LUA_NUMBER_INT64
#define BUILDINFO_BUILD_TYPE \"integer\"
#else
#ifdef CONFIG_LUA_NUMBER_DOUBLE
#define BUILDINFO_BUILD_TYPE \"double\"
#else
#define BUILDINFO_BUILD_TYPE \"float\"
#endif
#endif

#define USER_PROLOG \"@user_prolog@\"
#define BUILDINFO_BRANCH \"@branch@\"
#define BUILDINFO_COMMIT_ID \"@commit_id@\"
#define BUILDINFO_RELEASE \"@release@\"
#define BUILDINFO_RELEASE_DTS \"@release_dts@\"
#define BUILDINFO_BUILD_DATE \"@build_date@\"
#define BUILDINFO_MODULES \"@buildinfo_modules@\"
#define BUILDINFO_LFS_SIZE \@buildinfo_lfs_size@\

#endif	/* __BUILDINFO_H__ */
"
)

execute_process(
  COMMAND date "+%Y-%m-%d %H:%M"
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE build_date
)

execute_process(
  COMMAND git rev-parse HEAD
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE commit_id
)

execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  COMMAND sed -E "s/[\/\\]+/_/g"
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE branch
)

execute_process(
  COMMAND git describe --tags --long
  COMMAND sed -E "s/(.*)-(.*)-.*/\\1 +\\2/g"
  COMMAND sed "s/ +0$//"
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE release
)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E env TZ=UTC0 -- git show --quiet --date=format-local:%Y%m%d%H%M --format=%cd HEAD
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE release_dts
)

execute_process(
COMMAND grep "^lfs,.*" ${PROJECT_DIR}/components/platform/partitions.csv
COMMAND cut -d, -f5
COMMAND tr -d " "
COMMAND awk "{print strtonum( $1 )}"
OUTPUT_STRIP_TRAILING_WHITESPACE
OUTPUT_VARIABLE buildinfo_lfs_size
)

file(CONFIGURE OUTPUT ${PROJECT_DIR}/components/platform/include/buildinfo.h
  CONTENT "${buildinfo_h}"
  @ONLY
)
