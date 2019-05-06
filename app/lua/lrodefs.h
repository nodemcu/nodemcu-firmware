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
#define LREGISTER(L, name, table) return 0
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

#define LROT_TABLE(t)        static const LUA_REG_TYPE t ## _map[];
#define LROT_TABLEREF(t)     ((void *) t ## _map)
#define LROT_BEGIN(t)        static const LUA_REG_TYPE t ## _map [] = {
#define LROT_PUBLIC_BEGIN(t) const LUA_REG_TYPE t ## _map[] = {
#define LROT_EXTERN(t)       extern const LUA_REG_TYPE t ## _map[]
#define LROT_TABENTRY(n,t)   {LRO_STRKEY(#n), LRO_ROVAL(t ## _map)},
#define LROT_FUNCENTRY(n,f)  {LRO_STRKEY(#n), LRO_FUNCVAL(f)},
#define LROT_NUMENTRY(n,x)   {LRO_STRKEY(#n), LRO_NUMVAL(x)},
#define LROT_LUDENTRY(n,x)   {LRO_STRKEY(#n), LRO_LUDATA((void *) x)},
#define LROT_END(t,mt, f)    {LRO_NILKEY, LRO_NILVAL} };
#define LREGISTER(L, name, table) return 0

#endif /* lrodefs_h */

