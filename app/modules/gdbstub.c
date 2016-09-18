/*
 * Module for doing various horrible things 
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

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(GDBSTUB, "gdbstub", gdbstub_map, lgdbstub_open);
