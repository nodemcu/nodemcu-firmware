/* Bitwise operations library */
/* (c) Reuben Thomas 2000-2008 */
/* See README for license */

// Modified by BogdanM for eLua


#include "module.h"
#include "c_limits.h"

#include "lauxlib.h"

/* FIXME: Assume size_t is an unsigned lua_Integer */
typedef size_t lua_UInteger;
#define LUA_UINTEGER_MAX SIZE_MAX

/* Define TOBIT to get a bit value */
#define TOBIT(L, n)                    \
  (luaL_checkinteger((L), (n)))

/* Operations

   The macros MONADIC and VARIADIC only deal with bitwise operations.

   LOGICAL_SHIFT truncates its left-hand operand before shifting so
   that any extra bits at the most-significant end are not shifted
   into the result.

   ARITHMETIC_SHIFT does not truncate its left-hand operand, so that
   the sign bits are not removed and right shift work properly.
   */
  
#define MONADIC(name, op)                                       \
  static int bit_ ## name(lua_State *L) {                       \
    lua_pushinteger(L, op TOBIT(L, 1));                         \
    return 1;                                                   \
  }

#define VARIADIC(name, op)                      \
  static int bit_ ## name(lua_State *L) {       \
    int n = lua_gettop(L), i;                   \
    lua_Integer w = TOBIT(L, 1);                \
    for (i = 2; i <= n; i++)                    \
      w op TOBIT(L, i);                         \
    lua_pushinteger(L, w);                      \
    return 1;                                   \
  }

#define LOGICAL_SHIFT(name, op)                                         \
  static int bit_ ## name(lua_State *L) {                               \
    lua_pushinteger(L, (lua_UInteger)TOBIT(L, 1) op                     \
                          (unsigned)luaL_checknumber(L, 2));            \
    return 1;                                                           \
  }

#define ARITHMETIC_SHIFT(name, op)                                      \
  static int bit_ ## name(lua_State *L) {                               \
    lua_pushinteger(L, (lua_Integer)TOBIT(L, 1) op                      \
                          (unsigned)luaL_checknumber(L, 2));            \
    return 1;                                                           \
  }

MONADIC(bnot,  ~)
VARIADIC(band, &=)
VARIADIC(bor,  |=)
VARIADIC(bxor, ^=)
ARITHMETIC_SHIFT(lshift,  <<)
LOGICAL_SHIFT(rshift,     >>)
ARITHMETIC_SHIFT(arshift, >>)

// Lua: res = bit( position )
static int bit_bit( lua_State* L )
{
  lua_pushinteger( L, ( lua_Integer )( 1 << luaL_checkinteger( L, 1 ) ) );
  return 1;
}

// Lua: res = isset( value, position )
static int bit_isset( lua_State* L )
{
  lua_UInteger val = ( lua_UInteger )luaL_checkinteger( L, 1 );
  unsigned pos = ( unsigned )luaL_checkinteger( L, 2 );
  
  lua_pushboolean( L, val & ( 1 << pos ) ? 1 : 0 );
  return 1;
}

// Lua: res = isclear( value, position )
static int bit_isclear( lua_State* L )
{
  lua_UInteger val = ( lua_UInteger )luaL_checkinteger( L, 1 );
  unsigned pos = ( unsigned )luaL_checkinteger( L, 2 );
  
  lua_pushboolean( L, val & ( 1 << pos ) ? 0 : 1 );
  return 1;
}

// Lua: res = set( value, pos1, pos2, ... )
static int bit_set( lua_State* L )
{ 
  lua_UInteger val = ( lua_UInteger )luaL_checkinteger( L, 1 );
  unsigned total = lua_gettop( L ), i;
  
  for( i = 2; i <= total; i ++ )
    val |= 1 << ( unsigned )luaL_checkinteger( L, i );
  lua_pushinteger( L, ( lua_Integer )val );
  return 1;
}

// Lua: res = clear( value, pos1, pos2, ... )
static int bit_clear( lua_State* L )
{
  lua_UInteger val = ( lua_UInteger )luaL_checkinteger( L, 1 );
  unsigned total = lua_gettop( L ), i;
  
  for( i = 2; i <= total; i ++ )
    val &= ~( 1 << ( unsigned )luaL_checkinteger( L, i ) );
  lua_pushinteger( L, ( lua_Integer )val );
  return 1; 
}

static const LUA_REG_TYPE bit_map[] = {
  { LSTRKEY( "bnot" ),    LFUNCVAL( bit_bnot ) },
  { LSTRKEY( "band" ),    LFUNCVAL( bit_band ) },
  { LSTRKEY( "bor" ),     LFUNCVAL( bit_bor ) },
  { LSTRKEY( "bxor" ),    LFUNCVAL( bit_bxor ) },
  { LSTRKEY( "lshift" ),  LFUNCVAL( bit_lshift ) },
  { LSTRKEY( "rshift" ),  LFUNCVAL( bit_rshift ) },
  { LSTRKEY( "arshift" ), LFUNCVAL( bit_arshift ) },
  { LSTRKEY( "bit" ),     LFUNCVAL( bit_bit ) },
  { LSTRKEY( "set" ),     LFUNCVAL( bit_set ) },
  { LSTRKEY( "clear" ),   LFUNCVAL( bit_clear ) },
  { LSTRKEY( "isset" ),   LFUNCVAL( bit_isset ) },
  { LSTRKEY( "isclear" ), LFUNCVAL( bit_isclear ) },
  { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(BIT, "bit", bit_map, NULL);
