SECTIONS {
  .flash.rodata : ALIGN(4)
  {
    /* Link-time arrays containing the defs for the included modules */
    lua_libs_base = ABSOLUTE(.);
    KEEP(*(.lua_libs))
    LONG(0) LONG(0) /* Null-terminate the array */
    lua_rotable_base = ABSOLUTE(.);
    KEEP(*(.lua_rotable))
    LONG(0) LONG(0) /* Null-terminate the array */
    esp_event_cb_table = ABSOLUTE(.);
    KEEP(*(.esp_event_cb_table))
    LONG(0) LONG(0) /* Null-terminate the array */
    /* Reserved areas, flash page aligned and last */
    . = ALIGN(4096);
    KEEP(*(.irom.reserved .irom.reserved.*))
  }
}
INSERT BEFORE .flash.text
