idf_component_register(
  SRCS "ip_fmt.c" "lextra.c" "linit.c" "lnodeaux.c" "uart.c" "user_main.c"
  INCLUDE_DIRS "include"
  REQUIRES "lua"
  PRIV_REQUIRES "nvs_flash"
  LDFRAGMENTS "nodemcu.lf"
)
