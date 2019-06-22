
    /* ----- Begin NodeMCU link-time arrays ------- */

    . = ALIGN(4);
    /* Link-time arrays containing the defs for the included modules */
    lua_libs = ABSOLUTE(.);
    KEEP(*(.lua_libs))
    LONG(0) LONG(0) /* Null-terminate the array */
    lua_rotable = ABSOLUTE(.);
    KEEP(*(.lua_rotable))
    LONG(0) LONG(0) /* Null-terminate the array */
    esp_event_cb_table = ABSOLUTE(.);
    KEEP(*(.esp_event_cb_table))
    LONG(0) LONG(0) /* Null-terminate the array */

    /* ----- End NodeMCU link-time arrays ------- */

