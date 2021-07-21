idf_component_register(
  SRCS "ip_fmt.c" "lextra.c" "linit.c" "lnodeaux.c" "uart.c" "user_main.c"
  INCLUDE_DIRS "include"
  REQUIRES "lua"
  PRIV_REQUIRES "nvs_flash"
  LDFRAGMENTS "nodemcu.lf"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -DCONFIG_NODEMCU_CMODULE_UART)
target_link_libraries(${COMPONENT_LIB}
  "-u UART_module_selected1"
  "-u lua_rotables_part_map"
  "-Wl,-defsym=lua_rotables_map=_lua_rotables_map_start"
  "-Wl,-defsym=lua_libs_map=_lua_libs_map_start"
)
