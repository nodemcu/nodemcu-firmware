#ifndef modules_common_h
#define modules_common_h

#include "lua.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Some common code shared between modules */

/* Fetch an optional int value from a table on the top of the stack. If the
   option name is not present or the table is nil, returns default_val. Errors
   if the key name is present in the table but is not an integer.
*/
int opt_checkint(lua_State *L, const char *name, int default_val);

/* Like the above, but also specifies a range which the value must be within */
int opt_checkint_range(lua_State *L, const char *name, int default_val, int min_val, int max_val);

/* Fetch an optional bool value from a table on the top of the stack. If the
   option name is not present or the table is nil, returns default_val. Errors
   if the key name is present in the table but is not a boolean.
*/
bool opt_checkbool(lua_State *L, const char *name, bool default_val);

/* Fetch an optional string from a table on the top of the stack. If the
   option name is not present or the table is nil, returns default_val. Errors
   if the key name is present in the table but is not a string.

   Note that l is updated with the string length only when the option name
   resolves to a string. It is not updated when default_val is returned since
   it is not possible to determine the length of default_val in case it contains
   embedded \0.
*/
const char *opt_checklstring(lua_State *L, const char *name, const char *default_val, size_t *l);

/* Like luaL_argerror() but producing a more suitable error message */
int opt_error(lua_State *L, const char* name, const char *extramsg);

/* Returns true and pushes the value onto the stack, if name exists in the
   table on the top of the stack and is of type required_type. Errors if name
   is present but the wrong type. Returns false and pushes nothing if name is
   not present in the table, or the table is nil.
*/
bool opt_get(lua_State *L, const char *name, int required_type);

/* Macro like luaL_argcheck() for defining custom additional checks */
#define opt_check(L, cond, name, extramsg) \
    ((void)((cond) || opt_error(L, (name), (extramsg))))

#endif
