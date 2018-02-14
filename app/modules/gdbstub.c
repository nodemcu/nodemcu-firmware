/*
 * This module, when enabled with the LUA_USE_MODULES_GDBSTUB define causes
 * the gdbstub code to be included and enabled to handle all fatal exceptions.
 * This allows you to use the lx106 gdb to catch the exception and then poke
 * around. You can continue from a break, but attempting to continue from an 
 * exception usually fails. 
 *
 * This should not be included in production builds as any exception will
 * put the nodemcu into a state where it is waiting for serial input and it has
 * the watchdog disabled. Good for debugging. Bad for unattended operation!
 *
 * See the docs for more information.
 *
 * Philip Gladstone, N1DQ
 */

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_types.h"
#include "user_interface.h"
#include "../esp-gdbstub/gdbstub.h"

// gdbstub.brk()     just executes a break instruction. Enters gdb
static int lgdbstub_break(lua_State *L) {
  asm("break 0,0" ::);
  return 0;
}

// gdbstub.gdboutput(1)    switches the output to gdb format so that gdb can display it
static int lgdbstub_gdboutput(lua_State *L) {
  gdbstub_redirect_output(lua_toboolean(L, 1));
  return 0;
}

static int lgdbstub_open(lua_State *L) {
  gdbstub_init();
  return 0;
}

// Module function map
static const LUA_REG_TYPE gdbstub_map[] = {
  { LSTRKEY( "brk" ),    	LFUNCVAL( lgdbstub_break    ) },
  { LSTRKEY( "gdboutput" ),    	LFUNCVAL( lgdbstub_gdboutput) },
  { LSTRKEY( "open" ),    	LFUNCVAL( lgdbstub_open) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(GDBSTUB, "gdbstub", gdbstub_map, NULL);
