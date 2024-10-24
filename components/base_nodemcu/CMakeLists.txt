idf_component_register(
  SRCS "ip_fmt.c" "user_main.c"
  INCLUDE_DIRS "include"
  REQUIRES "lua"
  PRIV_REQUIRES "driver" "esp_netif" "esp_vfs_console" "nvs_flash" "vfs"
  LDFRAGMENTS "nodemcu.lf"
)
