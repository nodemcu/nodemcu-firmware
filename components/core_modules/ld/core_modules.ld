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

    /* Due to the way the gen_appbin.py script packs the bundle that goes into
     * flash, we don't have a convenient _flash_used_end symbol on the ESP32.
     * Instead we sum up the sections we know goes into the
     * irom0_flash.bin, so we can use that as a starting point to scan for
     * the next free page. We could presumably be even more specific and
     * account for the block headers and trailing checksum. Maybe later.
     */
    _irom0_bin_min_sz = ABSOLUTE(
      _rodata_end - _rodata_start +
      _lit4_end - _lit4_start +
      _text_end - _text_start +
      _data_end - _data_start);

  }
}
INSERT AFTER .flash.text
