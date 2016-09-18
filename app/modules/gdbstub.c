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

static int lgdbstub_break(lua_State *L) {
  asm("break 0,0" ::);
  return 0;
}

static int lgdbstub_open(lua_State *L) {
  gdbstub_init();
  return 0;
}

// Module function map
static const LUA_REG_TYPE gdbstub_map[] = {
  { LSTRKEY( "brk" ),    LFUNCVAL( lgdbstub_break    ) },

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(GDBSTUB, "gdbstub", gdbstub_map, lgdbstub_open);
