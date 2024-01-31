idf_component_register(
  SRCS "i2c_sw_master.c"
  INCLUDE_DIRS "include"
  REQUIRES "esp_rom"
  PRIV_REQUIRES "driver"
)
