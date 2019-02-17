/* Read-only tables helper */

#ifndef lrodefs_h
#define lrodefs_h

#include "lrotable.h"

#undef LUA_REG_TYPE
#undef LSTRKEY
#undef LNILKEY
#undef LNUMKEY
#undef LFUNCVAL
#undef LNUMVAL
#undef LROVAL
#undef LNILVAL
#undef LREGISTER

#if LUA_OPTIMIZE_MEMORY >=1
#define LUA_REG_TYPE                luaR_entry
#define LSTRKEY                     LRO_STRKEY
#define LNUMKEY                     LRO_NUMKEY
#define LNILKEY                     LRO_NILKEY
#define LFUNCVAL                    LRO_FUNCVAL
#define LUDATA                      LRO_LUDATA
#define LNUMVAL                     LRO_NUMVAL
#define LROVAL                      LRO_ROVAL
#define LNILVAL                     LRO_NILVAL
#define LREGISTER(L, name, table)\
  return 0
#else
#define LUA_REG_TYPE                luaL_reg
#define LSTRKEY(x)                  x
#define LNILKEY                     NULL
#define LFUNCVAL(x)                 x
#define LNILVAL                     NULL
#define LREGISTER(L, name, table)\
  luaL_register(L, name, table);\
  return 1
#endif

#define LROT_TABENTRY(n,t)  {LSTRKEY(#n), LRO_ROVAL(t)}
#define LROT_FUNCENTRY(n,f) {LSTRKEY(#n), LRO_FUNCVAL(f)}
#define LROT_NUMENTRY(n,x)  {LSTRKEY(#n), LRO_NUMVAL(x)}
#define LROT_END            {LNILKEY, LNILVAL}

#endif /* lrodefs_h */

