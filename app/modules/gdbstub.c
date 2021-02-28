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
#include <stdint.h>
#include "user_interface.h"
#include "../esp-gdbstub/gdbstub.h"

static int init_done = 0;
static int lgdbstub_open(lua_State *L);

// gdbstub.brk()  init gdb if nec and execute a break instructiont to entry gdb
static int lgdbstub_break(lua_State *L) {
  lgdbstub_open(L);
  asm("break 0,0" ::);
  return 0;
}

// as for break but also redirect output to the debugger.
static int lgdbstub_pbreak(lua_State *L) {
  lgdbstub_open(L);
  gdbstub_redirect_output(1);
  asm("break 0,0" ::);
  return 0;
}

// gdbstub.gdboutput(1)    switches the output to gdb format so that gdb can display it
static int lgdbstub_gdboutput(lua_State *L) {
  gdbstub_redirect_output(lua_toboolean(L, 1));
  return 0;
}

static int lgdbstub_open(lua_State *L) {
  if (init_done)
    return 0;
  gdbstub_init();
  init_done = 1;
  return 0;
}

// Module function map
LROT_BEGIN(gdbstub, NULL, 0)
  LROT_FUNCENTRY( brk, lgdbstub_break )
  LROT_FUNCENTRY( pbrk, lgdbstub_pbreak )
  LROT_FUNCENTRY( gdboutput, lgdbstub_gdboutput )
  LROT_FUNCENTRY( open, lgdbstub_open )
LROT_END(gdbstub, NULL, 0)


NODEMCU_MODULE(GDBSTUB, "gdbstub", gdbstub, NULL);
