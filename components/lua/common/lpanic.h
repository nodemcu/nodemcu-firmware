#ifndef _LUA_PANIC_H_
#define _LUA_PANIC_H_

#include "lua.h"

void panic_clear_nvval (void);
int panic_get_nvval (void);
int lpanic (lua_State *L);

#endif
