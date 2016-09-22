SECTIONS {
  .lua_lib : ALIGN(4)
  {
    /* Link-time arrays containing the defs for the included modules */
    lua_libs = ABSOLUTE(.);
    /* Allow either empty define or defined-to-1 to include the module */
    KEEP(*(.lua_libs))
    LONG(0) LONG(0) /* Null-terminate the array */
    lua_rotable = ABSOLUTE(.);
    KEEP(*(.lua_rotable))
    LONG(0) LONG(0) /* Null-terminate the array */
  }
}
INSERT AFTER .flash.text
