
    /* ----- Begin NodeMCU link-time arrays ------- */

    /* Don't change the alignment here without also updating the _Static_assert
     * over in linit.c! */
    . = ALIGN(8);
    /* Link-time arrays containing the defs for the included modules */
    lua_libs_map = ABSOLUTE(.);
    KEEP(*(.lua_libs))
    /* Null-terminate the array, 24 bytes */
    LONG(0) LONG(0) LONG(0) LONG(0) LONG(0) LONG(0)

    /* Don't change the alignment here without also updating the _Static_assert
     * over in linit.c! */
    . = ALIGN(8);
    lua_rotables_map = ABSOLUTE(.);
    KEEP(*(.lua_rotable))
    /* Null-terminate the array, 24 bytes */
    LONG(0) LONG(0) LONG(0) LONG(0) LONG(0) LONG(0)

    /* Don't change the alignment here without also updating the _Static_assert
     * over in nodemcu_esp_event.h! */
    . = ALIGN(4);
    esp_event_cb_table = ABSOLUTE(.);
    KEEP(*(.esp_event_cb_table))
    LONG(0) LONG(0) /* Null-terminate the array, 8 bytes */

    /* ----- End NodeMCU link-time arrays ------- */

