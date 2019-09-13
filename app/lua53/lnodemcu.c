
#define lnodemcu_c
#define LUA_CORE

#include "lua.h"
#include <string.h>

#include "lobject.h"
#include "lstate.h"
#include "lapi.h"
#include "lauxlib.h"
#include "lgc.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lnodemcu.h"
#ifdef LUA_USE_ESP
#include "platform.h"
#endif

#ifdef DEVELOPMENT_USE_GDB
/*
 *  This is a simple stub used by lua_assert() if DEVELOPMENT_USE_GDB is defined.
 *  Instead of crashing out with an assert error, this hook starts the GDB remote
 *  stub if not already running and then issues a break.  The rationale here is
 *  that when testing the developer might be using screen/PuTTY to work interactively
 *  with the Lua Interpreter via UART0.  However if an assert triggers, then there
 * is the option to exit the interactive session and start the Xtensa remote GDB
 * which will then sync up with the remote GDB client to allow forensics of the error.
 */
#ifdef LUA_USE_HOST
LUALIB_API void luaL_dbgbreak(void) {
  lua_writestring("debug break", sizeof("debug break")-1);  /* allows BT analysis of assert fails */
}
#else
extern void gdbstub_init(void);
extern void gdbstub_redirect_output(int);

LUALIB_API void luaL_dbgbreak(void) {
  static int repeat_entry = 0;
  if  (repeat_entry == 0) {
    dbg_printf("Start up the gdb stub if not already started\n");
    gdbstub_init();
    gdbstub_redirect_output(1);
    repeat_entry = 1;
  }
  asm("break 0,0" ::);
}
#endif

#endif

LUA_API int lua_freeheap (void) {
#ifdef LUA_USE_HOST
  return MAX_INT;
#else
  return (int) platform_freeheap();
#endif
}

LUA_API int lua_getstrings(lua_State *L, int opt) {
  stringtable *tb = NULL;
  Table *t;
  int i, j, n = 0;

  if (n == 0)
    tb = &G(L)->strt;
#ifdef LUA_USE_ESP
  else if (n == 1 && G(L)->ROstrt.hash)
    tb = &G(L)->ROstrt;
#endif
  if (tb == NULL)
   return 0;

  lua_lock(L);
  t = luaH_new(L);
  sethvalue(L, L->top, t);
  api_incr_top(L);
  luaH_resize(L, t, tb->nuse, 0);
  luaC_checkGC(L);
  lua_unlock(L);

  for (i = 0, j = 1; i < tb->size; i++) {
    TString *o;
    for(o = tb->hash[i]; o; o = cast(TString *,o->next)) {
      TValue s;
      setsvalue(L, &s, o);
      luaH_setint(L, hvalue(L->top-1), j++, &s);  /* table[n] = true */
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
        if(tsvalue(L->top-1)==G(L)->tmname[i]) {
          flags |= cast_byte(1u<<i);
          break;
        }
      }
      lua_pop(L,1);
    }
  }
  t->next      = (GCObject *)1;
  t->tt        = LUA_TTBLROF;
  t->marked    = LROT_MARKED;
  t->flags     = flags;
  t->lsizenode = i;
  t->metatable = cast(Table *, mt);
  t->entry     = cast(ROTable_entry *, e);
}

LUAI_FUNC void luaN_init (lua_State *L) {
}

LUAI_FUNC int  luaN_reload_reboot (lua_State *L) {
  return 0;
}

LUAI_FUNC int  luaN_index (lua_State *L) {
  return 0;
}
