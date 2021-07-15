foreach(def
  "-DU8X8_USE_PINS"
  "-DU8X8_WITH_USER_PTR"
)
    idf_build_set_property(COMPILE_DEFINITIONS ${def} APPEND)
endforeach()
