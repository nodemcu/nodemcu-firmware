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
#include "lnodemcu.h"

//== NodeMCU lauxlib.h API extensions ========================================//
#ifdef LUA_USE_ESP
#include "platform.h"
/*
** Error Reporting Task.  We can't pass a string parameter to the error reporter
** directly through the task interface the call is wrapped in a C closure with
** the error string as an Upval and this is posted to call the Lua reporter.
*/
static int errhandler_aux (lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "onerror");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    lua_getglobal(L, "print");
  }
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_call(L, 1, 0);  /* Using an error handler would cause an infinite loop! */
  return 0;
}

/*
** Error handler for luaL_pcallx(), called from the lua_pcall() with a single 
** argument -- the thrown error. This plus depth=2 is passed to debug.traceback()
** to convert into an error message which it handles in a separate posted task.
*/
static int errhandler (lua_State *L) {
  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "traceback");
  if (lua_isfunction(L, -1)) {
    lua_insert(L, 1);                 /* insert tracback function above error */
    lua_pop(L, 1);                              /* dump the debug table entry */
    lua_pushinteger(L, 2);                /* skip this function and traceback */
    lua_call(L, 2, 1);      /* call debug.traceback and return it as a string */
    lua_pushcclosure(L, errhandler_aux, 1);       /* report with str as upval */
    luaL_posttask(L, LUA_TASK_HIGH);
  }
  return 0;
}

/*
** Use in CBs and other C functions to call a Lua function. This includes
** an error handler which will catch any error and then post this to the
** registered reporter function as a separate follow-on task.
*/
LUALIB_API int luaL_pcallx (lua_State *L, int narg, int nres) { // [-narg, +0, v]
  int status;
  int base = lua_gettop(L) - narg;
  lua_pushcfunction(L, errhandler);
  lua_insert(L, base);                                      /* put under args */
  status = lua_pcall(L, narg, nres, base);
  lua_remove(L, base);                           /* remove traceback function */
  if (status != LUA_OK && status != LUA_ERRRUN) {  
    lua_gc(L, LUA_GCCOLLECT, 0);   /* call onerror directly if handler failed */
    lua_pushliteral(L, "out of memory");
    lua_pushcclosure(L, errhandler_aux, 1);            /* report EOM as upval */
    luaL_posttask(L, LUA_TASK_HIGH);
    }
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
  luaL_checkfunction(L, -1);
  luaL_unref(L, LUA_REGISTRYINDEX, (int) task_fn_ref);
  lua_pushinteger(L, prio);
  luaL_pcallx(L, 1, 0);
}

/*
** Schedule a Lua function for task execution
*/
LUALIB_API int luaL_posttask( lua_State* L, int prio ) {          // [-1, +0, -]
  if (!task_handle)
    task_handle = platform_task_get_id(do_task);

  if (!lua_isfunction(L, -1) || prio < LUA_TASK_LOW|| prio > LUA_TASK_HIGH)
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
#else
LUALIB_API int luaL_posttask( lua_State* L, int prio ) { 
  return 0;
} /* Dummy stub on host */
#endif

#ifdef LUA_USE_ESP
/*
 * Return an LFS function
 */
LUALIB_API int luaL_pushlfsmodule (lua_State *L) {
  if (lua_pushlfsindex(L) == LUA_TNIL) {
    lua_remove(L,-2);  /* dump the name to balance the stack */
    return 0;          /* return nil if LFS not loaded */
  }
  lua_insert(L, -2);
  lua_call(L, 1, 1);
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    lua_pushnil(L);  /* replace DTS by nil */
  }
  return 1;
}

/*
 * Return an array of functions in LFS
 */
LUALIB_API int luaL_pushlfsmodules (lua_State *L) {
  if (lua_pushlfsindex(L) == LUA_TNIL)
    return 0;              /* return nil if LFS not loaded */
  lua_call(L, 0, 2);
  lua_remove(L, -2);     /* remove DTS leaving array */
  return 1;

}

/*
 * Return the Unix timestamp of the LFS image creation
 */
LUALIB_API int luaL_pushlfsdts (lua_State *L) {
  if (lua_pushlfsindex(L) == LUA_TNIL)
    return 0;              /* return nil if LFS not loaded */
  lua_call(L, 0, 1);
  return 1;
}
#endif

//== NodeMCU lua.h API extensions ============================================//

LUA_API int lua_freeheap (void) {
#ifdef LUA_USE_HOST
  return MAX_INT;
#else
  return (int) platform_freeheap();
#endif
}

#define api_incr_top(L)   {api_check(L, L->top < L->ci->top); L->top++;}
LUA_API int lua_pushstringsarray(lua_State *L, int opt) {
  stringtable *tb = NULL;
  int i, j;

  lua_lock(L);
  if (opt == 0)
    tb = &G(L)->strt;
#ifdef LUA_USE_ESP
  else if (opt == 1 && G(L)->ROstrt.hash)
    tb = &G(L)->ROstrt;
#endif
  if (tb == NULL) {
    setnilvalue(L->top);
    api_incr_top(L);
    lua_unlock(L);
    return 0;
  }

  Table *t = luaH_new(L, tb->nuse, 0);
  sethvalue(L, L->top, t);
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);

  for (i = 0, j = 1; i < tb->size; i++) {
    GCObject *o;
    for(o = tb->hash[i]; o; o = o->gch.next) {
      TValue v;
      setsvalue(L, &v, o); 
      setobj2s(L, luaH_setnum(L, hvalue(L->top-1), j++), &v);
    }
  }
  return 1;
}

LUA_API void lua_createrotable (lua_State *L, ROTable *t, const ROTable_entry *e, ROTable *mt) {
  int i, j;
  lu_byte flags = ~0;
  const char *plast = (char *)"_";
  for (i = 0; e[i].key; i++) {
    if (e[i].key[0] == '_' && strcmp(e[i].key,plast)) {
      plast = e[i].key;
      lua_pushstring(L,e[i].key);
      for (j=0; j<TM_EQ; j++){
        if(rawtsvalue(L->top-1)==G(L)->tmname[i]) {
          flags |= cast_byte(1u<<i);
          break;
        }
      }
      lua_pop(L,1);
    }
  }
  t->next      = (GCObject *)1;
  t->tt        = LUA_TROTABLE;
  t->marked    = LROT_MARKED;
  t->flags     = flags;
  t->lsizenode = i;
  t->metatable = cast(Table *, mt);
  t->entry     = cast(ROTable_entry *, e);
}

#ifdef LUA_USE_ESP
#include "lfunc.h"

/* Push the LClosure of the LFS index function */
LUA_API int lua_pushlfsindex (lua_State *L) {
  lua_lock(L);
  Proto *p = G(L)->ROpvmain;
  if (p) {
    Closure *cl = luaF_newLclosure(L, 0, hvalue(gt(L)));
    cl->l.p = p;
    setclvalue(L, L->top, cl);
  } else {
    setnilvalue(L->top);
  }
  api_incr_top(L);
  lua_unlock(L);
  return p ? LUA_TFUNCTION : LUA_TNIL;
}
#endif
