idf_component_register(
  SRCS "dht.c" "flash_api.c" "flash_fs.c" "onewire.c" "platform.c"
    "platform_flash.c" "platform_partition.c" "platform_rmt.c"
    "u8x8_nodemcu_hal.c" "ucg_nodemcu_hal.c" "vfs.c" "wdt.c" "ws2812.c"
  INCLUDE_DIRS "include"
  REQUIRES "spiffs" "u8g2" "ucg" "driver_i2c" "task"
  PRIV_REQUIRES "bootloader_support" "lua" "esp32"
)
