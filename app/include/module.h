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

#define MODULE_EXPAND_(x) x
#define MODULE_PASTE_(x,y) x##y
#define MODULE_EXPAND_PASTE_(x,y) MODULE_PASTE_(x,y)

#ifdef LUA_CROSS_COMPILER
#define LOCK_IN_SECTION(s) __attribute__((used,unused,section(".rodata1." #s)))
#else
#define LOCK_IN_SECTION(s) __attribute__((used,unused,section(".lua_" #s)))
#endif
/* For the ROM table, we name the variable according to ( | denotes concat):
 *   cfgname | _module_selected | LUA_USE_MODULES_##cfgname
 * where the LUA_USE_MODULES_XYZ macro is first expanded to yield either
 * an empty string (or 1) if the module has been enabled, or the literal
 * LUA_USE_MOUDLE_XYZ in the case it hasn't. Thus, the name of the variable
 * ends up looking either like XYZ_module_enabled, or if not enabled,
 * XYZ_module_enabledLUA_USE_MODULES_XYZ.  This forms the basis for
 * letting the build system detect automatically (via nm) which modules need
 * to be linked in.
 */
#define NODEMCU_MODULE(cfgname, luaname, map, initfunc) \
  const LOCK_IN_SECTION(libs) \
    luaL_Reg MODULE_PASTE_(lua_lib_,cfgname) = { luaname, initfunc }; \
  const LOCK_IN_SECTION(rotable) \
    luaR_table MODULE_EXPAND_PASTE_(cfgname,MODULE_EXPAND_PASTE_(_module_selected,MODULE_PASTE_(LUA_USE_MODULES_,cfgname))) \
    = { luaname, map }


/* System module registration support, not using LUA_USE_MODULES_XYZ. */
#define BUILTIN_LIB_INIT(name, luaname, initfunc) \
  const LOCK_IN_SECTION(libs) \
    luaL_Reg MODULE_PASTE_(lua_lib_,name) = { luaname, initfunc }

#define BUILTIN_LIB(name, luaname, map) \
  const LOCK_IN_SECTION(rotable) \
    luaR_table MODULE_PASTE_(lua_rotable_,name) = { luaname, map }

#if !defined(LUA_CROSS_COMPILER) && !(MIN_OPT_LEVEL==2 && LUA_OPTIMIZE_MEMORY==2)
# error "NodeMCU modules must be built with LTR enabled (MIN_OPT_LEVEL=2 and LUA_OPTIMIZE_MEMORY=2)"
#endif

#endif
