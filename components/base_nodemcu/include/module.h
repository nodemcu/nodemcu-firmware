#ifndef __MODULE_H__
#define __MODULE_H__

#include "sdkconfig.h"
#include "lrotable.h"

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
 * When you've done this, you can simply add a Kconfig entry for the module
 * to control whether it gets linked in or not. With it linked in, you can
 * then within NodeMCU access it with myname.foo(), assuming you have
 * a foo function in your module.
 */

#define MODULE_EXPAND_(x) x
#define MODULE_PASTE_(x,y) x##y
#define MODULE_EXPAND_PASTE_(x,y) MODULE_PASTE_(x,y)

#ifdef LUA_CROSS_COMPILER
#ifdef _MSC_VER
//on msvc it is necessary to go through more pre-processor hoops to get the
//section name built; string merging does not happen in the _declspecs.
//NOTE: linker magic is invoked via the magical '$' character. Caveat editor.
#define __TOKIFY(s) .rodata1$##s
#define __TOTOK(s) __TOKIFY(s)
#define __STRINGIFY(x) #x
#define __TOSTRING(x) __STRINGIFY(x)
#define __ROSECNAME(s) __TOSTRING(__TOTOK(s))
#define LOCK_IN_SECTION(s) __declspec ( allocate( __ROSECNAME(s) ) )
#else
#define LOCK_IN_SECTION(s) __attribute__((used,unused,section(".rodata1." #s)))
#endif
#else
#define LOCK_IN_SECTION(s) __attribute__((used,unused,section(".lua_" #s)))
#endif
/* For the ROM table, we name the variable according to ( | denotes concat):
 *   cfgname | _module_selected | CONFIG_NODEMCU_CMODULE_##cfgname
 * where the CONFIG_NODEMCU_CMODULE_XYZ macro is first expanded to yield either
 * an empty string (or 1) if the module has been enabled, or the literal
 * CONFIG_NODEMCU_CMODULE_XYZ in the case it hasn't. Thus, the name of the variable
 * ends up looking either like XYZ_module_enabled, or if not enabled,
 * XYZ_module_enabledCONFIG_NODEMCU_CMODULE_XYZ.  This forms the basis for
 * letting the build system detect automatically (via nm) which modules need
 * to be linked in.
 */
#define NODEMCU_MODULE(cfgname, luaname, map, initfunc) \
  const LOCK_IN_SECTION(libs) \
    luaR_entry MODULE_PASTE_(lua_lib_,cfgname) = { luaname, LRO_FUNCVAL(initfunc) }; \
  const LOCK_IN_SECTION(rotable) \
    luaR_entry MODULE_EXPAND_PASTE_(cfgname,MODULE_EXPAND_PASTE_(_module_selected,MODULE_PASTE_(CONFIG_NODEMCU_CMODULE_,cfgname))) \
    = {luaname, LRO_ROVAL(map ## _map)}
#endif


// helper stringing macros
#define xstr(s) str(s)
#define str(s) #s

// EXTMODNAME is injected by the generated component.mk
#ifdef EXTMODNAME
#define MODNAME xstr(EXTMODNAME)
#else
#define MODNAME "module"
#endif

// use NODEMCU_MODULE_METATABLE() to generate a unique metatable name for your objects:
#define NODEMCU_MODULE_METATABLE() MODULE_EXPAND_(MODNAME xstr(__COUNTER__))

// NODEMCU_MODULE_STD() defines the entry points for an external module:
#define NODEMCU_MODULE_STD()                                             \
    static const LOCK_IN_SECTION(libs)                                   \
        luaR_entry lua_lib_module = {MODNAME, LRO_FUNCVAL(module_init)}; \
    const LOCK_IN_SECTION(rotable)                                       \
        luaR_entry MODULE_EXPAND_PASTE_(EXTMODNAME, _entry) = {MODNAME, LRO_ROVAL(module_map)};
