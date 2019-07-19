/*
** $Id: ltm.c,v 2.8.1.1 2007/12/27 13:02:25 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/


#define lnodemcu_c
#define LUA_CORE

#include "lua.h"
#include <string.h>

#include "lobject.h"
#include "lstate.h"
#include "lauxlib.h"
#include "lgc.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lrotable.h"
#include "platform.h"

extern int debug_errorfb (lua_State *L);
/*
** Error Reporting Task.  We can't pass a string parameter to the error reporter
** directly through the task interface the call is wrapped in a C closure with
** the error string as an Upval and this is posted to call the Lua reporter.
*/
static int report_traceback (lua_State *L) {
// **Temp**  lua_rawgeti(L, LUA_REGISTRYINDEX, G(L)->error_reporter);
  lua_getglobal(L, "print");
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_call(L, 1, 0);  /* Using an error handler would cause an infinite loop! */ 
  return 0;
}

/*
** Catch all error handler for CB calls.  This uses debug.traceback() to
** generate a full Lua traceback.  
*/
int luaN_traceback (lua_State *L) {
  if (lua_isstring(L, 1)) {
    lua_pushlightfunction(L, &debug_errorfb);
    lua_pushvalue(L, 1);                                /* pass error message */
    lua_pushinteger(L, 2);                /* skip this function and traceback */
    lua_call(L, 2, 1);      /* call debug.traceback and return it as a string */
    lua_pushcclosure(L, report_traceback, 1);     /* report with str as upval */
    luaN_posttask(L, LUA_TASK_HIGH);
  } 
  return 0;
}

/*
** Use in CBs and other C functions to call a Lua function. This includes
** an error handler which will catch any error and then post this to the 
** registered reporter function as a separate follow-on task.
*/
int luaN_call (lua_State *L, int narg, int nres, int doGC) { // [-narg, +0, v]
  int status;
  int base = lua_gettop(L) - narg;
  lua_pushcfunction(L, luaN_traceback);
  lua_insert(L, base);                                      /* put under args */
  status = lua_pcall(L, narg, (nres < 0 ? LUA_MULTRET : nres), base);
  lua_remove(L, base);                           /* remove traceback function */
  if (status && nres >=0)
    lua_settop(L, base + nres);                 /* balance the stack on error */ 
  /* force a complete garbage collection if requested */
  if (doGC) 
    lua_gc(L, LUA_GCCOLLECT, 0);
  return status;
}

static platform_task_handle_t task_handle = 0;

/*
** Task callback handler. Uses luaN_call to do a protected call with full traceback
*/
static void do_task (platform_task_param_t task_fn_ref, uint8_t prio) {
  lua_State* L = lua_getstate();
  if (prio < 0|| prio > 2)
    luaL_error(L, "invalid posk task");

/* Pop the CB func from the Reg */
//dbg_printf("calling Reg[%u]\n", task_fn_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, (int) task_fn_ref);
  luaL_checkanyfunction(L, -1);
  luaL_unref(L, LUA_REGISTRYINDEX, (int) task_fn_ref);
  lua_pushinteger(L, prio);
  luaN_call (L, 1, 0, 1);
}

/*
** Schedule a Lua function for task execution
*/
#include "lstate.h" /*DEBUG*/
LUA_API int luaN_posttask( lua_State* L, int prio ) {            // [-1, +0, -]
  if (!task_handle)
    task_handle = platform_task_get_id(do_task);

  if (!lua_isanyfunction(L, -1) || prio < LUA_TASK_LOW|| prio > LUA_TASK_HIGH)
    luaL_error(L, "invalid posk task");
//void *cl = clvalue(L->top-1);
  int task_fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);
//dbg_printf("posting Reg[%u]=%p\n",task_fn_ref,cl);
  if(!platform_post(prio, task_handle, (platform_task_param_t)task_fn_ref)) {
    luaL_unref(L, LUA_REGISTRYINDEX, task_fn_ref);
    luaL_error(L, "Task queue overflow. Task not posted");
  }
  return task_fn_ref;
}

