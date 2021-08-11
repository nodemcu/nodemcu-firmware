if(NOT "${IDF_TARGET}" STREQUAL "esp32c3")

idf_component_register(
  SRCS "CAN.c"
  INCLUDE_DIRS "include"
)

endif()
