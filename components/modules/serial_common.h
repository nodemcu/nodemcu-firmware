#ifndef SERIAL_COMMON_H
#define SERIAL_COMMON_H

#include "lua.h"

#include <stdint.h>
#include <stdbool.h>

struct serial_input_cfg;
typedef struct serial_input_cfg serial_input_cfg_t;

/**
 * Instantiate a new serial_input object.
 * @returns a freshly allocated serial input object, with no further resources
 *   associated with it.
 */
serial_input_cfg_t *serial_input_new(void);

/**
 * Free a serial_input_cfg_t object.
 * Releases all associated resources. The object may not be passed to any
 * serial_input_xxx functions after this.
 *
 * Must only be called from the Lua VM task context.
 */
void serial_input_free(lua_State *L, serial_input_cfg_t *cfg);


/**
 * Helper function to hand registration of "data" and "error" callbacks.
 * Expects the following calling signature:
 *   on("method", [number/char], function)
 *
 * Must only be called from the Lua VM task context.
 *
 * @param L The current Lua VM.
 * @param cfg Instance to un/register with. Must've been initialised with
 *   @c serial_input_init() originally.
 * @return Zero. Will luaL_error() on invalid args.
 */
int serial_input_register(lua_State *L, serial_input_cfg_t *cfg);

/**
 * Feed data into a serial_input stream for processing.
 *
 * Must only be called from the Lua VM task context, as it will invoke
 * Lua callbacks as necessary.
 * Uses lua_getstate() to obtain the LVM instance.
 *
 * @param cfg The serial_input instance.
 * @param buf The data buffer from which to feed bytes.
 * @param len The number of bytes available in the buffer.
 */
void serial_input_feed_data(serial_input_cfg_t *cfg, const char *buf, size_t len);

/**
 * Checks whether a "data" callback is registered.
 *
 * @param cfg The serial_input instance.
 * @return Whether a "data" callback is currently registered.
 */
bool serial_input_has_data_cb(serial_input_cfg_t *cfg);

/**
 * Direct access to invoking a configured "data" callback.
 *
 * Must only be called from the Lua VM task context.
 * Uses lua_getstate() to obtain the LVM instance.
 *
 * @param cfg The serial_input instance.
 * @param buf The data which to pass to the callback.
 * @param len The number of bytes available in the buffer.
 * @return True if the callback was successfully invoked (registered, and valid
 *   non-empty data passed).
 */
bool serial_input_dispatch_data(serial_input_cfg_t *cfg, const char *buf, size_t len);

/**
 * Direct access to invoking a configured "error" callback.
 * Must only be called from the Lua VM task context.
 * Uses lua_getstate() to obtain the LVM instance.
 *
 * @param cfg The serial_input instance.
 * @param msg The message to pass to the error callback.
 * @param len The length of the message, in bytes.
 * @return True if the callback was successfully invoked (registered, and valid
 *   non-empty data passed).
 */
bool serial_input_report_error(serial_input_cfg_t *cfg, const char *msg, size_t len);

#endif
