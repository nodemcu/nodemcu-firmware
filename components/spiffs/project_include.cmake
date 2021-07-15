foreach(def 
  "-Du32_t=uint32_t"
  "-Du16_t=uint16_t"
  "-Du8_t=uint8_t"
  "-Ds32_t=int32_t"
  "-Ds16_t=int16_t"
  "-Duint32=uint32_t"
  "-Duint16=uint16_t"
  "-Duint8=uint8_t"
  "-Dsint32=int32_t"
  "-Dsint16=int16_t"
  "-Dsint8=int8_t"
)
    idf_build_set_property(COMPILE_DEFINITIONS ${def} APPEND)
endforeach()

