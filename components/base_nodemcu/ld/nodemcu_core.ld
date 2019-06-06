SECTIONS {
  .flash.rodata : ALIGN(4)
  {
    /* Link-time arrays containing the defs for the included modules */
    lua_libs_map = ABSOLUTE(.);
    KEEP(*(.lua_libs))
    LONG(0) LONG(0) /* Null-terminate the array */
    lua_rotables_map = ABSOLUTE(.);
    KEEP(*(.lua_rotable))
    LONG(0) LONG(0) /* Null-terminate the array */
    esp_event_cb_table = ABSOLUTE(.);
    KEEP(*(.esp_event_cb_table))
    LONG(0) LONG(0) /* Null-terminate the array */
  }
}
INSERT BEFORE .flash.text

SECTIONS {
  .flash.lfs : ALIGN(4096)
  {
    /* Reserved areas, flash page aligned */
    KEEP(*(.irom.reserved .irom.reserved.*))
  }
}
INSERT AFTER .flash.text
