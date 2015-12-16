#ifndef __MODULE_H__
#define __MODULE_H__

#include "user_modules.h"
#include "lrodefs.h"

/* Registering a module within NodeMCU is really easy these days!
 *
 * Most of the work is done by a combination of pre-processor, compiler
 * and linker "magic". Gone are the days of needing to update 4+ separate
 * files just to register a module!
 *
 * You will need:
 *   - to include this header
 *   - a name for the module
 *   - a LUA_REG_TYPE module map
 *   - optionally, an init function
 *
 * Then simply put a line like this at the bottom of your module file:
 *
 *   NODEMCU_MODULE(MYNAME, "myname", myname_map, luaopen_myname);
 *
 * or perhaps
 *
 *   NODEMCU_MODULE(MYNAME, "myname", myname_map, NULL);
 *
 * if you don't need an init function.
 *
 * When you've done this, the module can be enabled in user_modules.h with:
 *
 *   #define LUA_USE_MODULES_MYNAME
 *
 * and within NodeMCU you access it with myname.foo(), assuming you have
 * a foo function in your module.
 */

#define MODULE_STRIFY__(x) #x
#define MODULE_STRIFY_(x) MODULE_STRIFY__(x)
#define MODULE_EXPAND_(x) x
#define MODULE_PASTE_(x,y) x##y

/* Given an uppercase name XYZ, look at the corresponding LUA_USE_MODULES_XYZ
 * and append that to the section name prefix (e.g. "lua_libs"). For an
 * included module, this will yield either ".lua_libs" (for an empty #define)
 * or ".lua_libs1" (if #defined to 1). The linker script will then
 * pick up all items in those sections and construct an array for us.
 * A non-included module ZZZ ends up in a section named
 * ".lua_libsLUA_USE_MODULES_ZZZ" which gets discarded by the linker (together
 * with the associated module code).
 */
#define PICK_SECTION_(pfx, name) \
   pfx MODULE_STRIFY_(MODULE_EXPAND_(LUA_USE_MODULES_ ## name))

#define NODEMCU_MODULE(cfgname, luaname, map, initfunc) \
  static const __attribute__((used,unused,section(PICK_SECTION_(".lua_libs",cfgname)))) \
    luaL_Reg MODULE_PASTE_(lua_lib_,cfgname) = { luaname, initfunc }; \
  static const __attribute__((used,unused,section(PICK_SECTION_(".lua_rotable",cfgname)))) \
    luaR_table MODULE_PASTE_(lua_rotable_,cfgname) = { luaname, map }


/* System module registration support, not using LUA_USE_MODULES_XYZ. */
#define BUILTIN_LIB_INIT(name, luaname, initfunc) \
  static const __attribute__((used,unused,section(".lua_libs"))) \
    luaL_Reg MODULE_PASTE_(lua_lib_,name) = { luaname, initfunc }

#define BUILTIN_LIB(name, luaname, map) \
  static const __attribute__((used,unused,section(".lua_rotable"))) \
    luaR_table MODULE_PASTE_(lua_rotable_,name) = { luaname, map }

#if !(MIN_OPT_LEVEL==2 && LUA_OPTIMIZE_MEMORY==2)
# error "NodeMCU modules must be built with LTR enabled (MIN_OPT_LEVEL=2 and LUA_OPTIMIZE_MEMORY=2)"
#endif

#endif
