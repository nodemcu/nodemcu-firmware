/*
** $Id: lmathlib.c,v 1.67.1.1 2007/12/27 13:02:25 roberto Exp $
** Standard mathematical library
** See Copyright Notice in lua.h
*/


#define lmathlib_c
#define LUA_LIB
#define LUAC_CROSS_FILE

#include "lua.h"
#include <stdlib.h>
#include <math.h>

#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"

#undef PI
#define PI (3.14159265358979323846)
#define RADIANS_PER_DEGREE (PI/180.0)



static int math_abs (lua_State *L) {
#ifdef LUA_NUMBER_INTEGRAL
  lua_Number x = luaL_checknumber(L, 1);
  if (x < 0) x = -x;	//fails for -2^31
  lua_pushnumber(L, x);
#else
  lua_pushnumber(L, fabs(luaL_checknumber(L, 1)));
#endif
  return 1;
}

#ifndef LUA_NUMBER_INTEGRAL
#if 0
static int math_sin (lua_State *L) {
  lua_pushnumber(L, sin(luaL_checknumber(L, 1)));
  return 1;
}

static int math_sinh (lua_State *L) {
  lua_pushnumber(L, sinh(luaL_checknumber(L, 1)));
  return 1;
}

static int math_cos (lua_State *L) {
  lua_pushnumber(L, cos(luaL_checknumber(L, 1)));
  return 1;
}

static int math_cosh (lua_State *L) {
  lua_pushnumber(L, cosh(luaL_checknumber(L, 1)));
  return 1;
}

static int math_tan (lua_State *L) {
  lua_pushnumber(L, tan(luaL_checknumber(L, 1)));
  return 1;
}

static int math_tanh (lua_State *L) {
  lua_pushnumber(L, tanh(luaL_checknumber(L, 1)));
  return 1;
}

static int math_asin (lua_State *L) {
  lua_pushnumber(L, asin(luaL_checknumber(L, 1)));
  return 1;
}

static int math_acos (lua_State *L) {
  lua_pushnumber(L, acos(luaL_checknumber(L, 1)));
  return 1;
}

static int math_atan (lua_State *L) {
  lua_pushnumber(L, atan(luaL_checknumber(L, 1)));
  return 1;
}

static int math_atan2 (lua_State *L) {
  lua_pushnumber(L, atan2(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
  return 1;
}
#endif

static int math_ceil (lua_State *L) {
  lua_pushnumber(L, ceil(luaL_checknumber(L, 1)));
  return 1;
}

static int math_floor (lua_State *L) {
  lua_pushnumber(L, floor(luaL_checknumber(L, 1)));
  return 1;
}
#if 0
static int math_fmod (lua_State *L) {
  lua_pushnumber(L, fmod(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
  return 1;
}

static int math_modf (lua_State *L) {
  double ip;
  double fp = modf(luaL_checknumber(L, 1), &ip);
  lua_pushnumber(L, ip);
  lua_pushnumber(L, fp);
  return 2;
}
#endif

#else  // #ifndef LUA_NUMBER_INTEGRAL

// In integer math, floor() and ceil() give the same value;
// having them in the integer library allows you to write code that
// works in both integer and floating point versions of Lua.
// This identity function is used for them.

static int math_identity (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1));
  return 1;
}

#endif // #ifndef LUA_NUMBER_INTEGRAL

#ifdef LUA_NUMBER_INTEGRAL
// Integer square root for integer version
static lua_Number isqrt(lua_Number x)
{
  lua_Number op, res, one;

  op = x; res = 0;

  /* "one" starts at the highest power of four <= than the argument. */
  one = 1 << 30;  /* second-to-top bit set */
  while (one > op) one >>= 2;

  while (one != 0) {
    if (op >= res + one) {
      op = op - (res + one);
      res = res +  2 * one;
    }
    res >>= 1;
    one >>= 2;
  }
  return(res);
}
#endif

static int math_sqrt (lua_State *L) {
#ifdef LUA_NUMBER_INTEGRAL
  lua_Number x = luaL_checknumber(L, 1);
  luaL_argcheck(L, 0<=x, 1, "negative");
  lua_pushnumber(L, isqrt(x));
#else
  lua_pushnumber(L, sqrt(luaL_checknumber(L, 1)));
#endif
  return 1;
}

#ifdef LUA_NUMBER_INTEGRAL
# define pow(a,b) luai_ipow(a,b)
#endif

static int math_pow (lua_State *L) {
  lua_pushnumber(L, pow(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
  return 1;
}

#ifdef LUA_NUMBER_INTEGRAL
# undef pow
#endif


#ifndef LUA_NUMBER_INTEGRAL
#if 0
static int math_log (lua_State *L) {
  lua_pushnumber(L, log(luaL_checknumber(L, 1)));
  return 1;
}

static int math_log10 (lua_State *L) {
  lua_pushnumber(L, log10(luaL_checknumber(L, 1)));
  return 1;
}

static int math_exp (lua_State *L) {
  lua_pushnumber(L, exp(luaL_checknumber(L, 1)));
  return 1;
}

static int math_deg (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1)/RADIANS_PER_DEGREE);
  return 1;
}

static int math_rad (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1)*RADIANS_PER_DEGREE);
  return 1;
}

static int math_frexp (lua_State *L) {
  int e;
  lua_pushnumber(L, frexp(luaL_checknumber(L, 1), &e));
  lua_pushinteger(L, e);
  return 2;
}

static int math_ldexp (lua_State *L) {
  lua_pushnumber(L, ldexp(luaL_checknumber(L, 1), luaL_checkint(L, 2)));
  return 1;
}
#endif

#endif // #ifdef LUA_NUMBER_INTEGRAL

static int math_min (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  lua_Number dmin = luaL_checknumber(L, 1);
  int i;
  for (i=2; i<=n; i++) {
    lua_Number d = luaL_checknumber(L, i);
    if (d < dmin)
      dmin = d;
  }
  lua_pushnumber(L, dmin);
  return 1;
}


static int math_max (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  lua_Number dmax = luaL_checknumber(L, 1);
  int i;
  for (i=2; i<=n; i++) {
    lua_Number d = luaL_checknumber(L, i);
    if (d > dmax)
      dmax = d;
  }
  lua_pushnumber(L, dmax);
  return 1;
}


#ifdef LUA_NUMBER_INTEGRAL

static int math_random (lua_State *L) {
  lua_Number r = (lua_Number)(rand()%RAND_MAX);

  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, 0);  /* Number between 0 and 1 - always 0 with ints */
      break;
    }
    case 1: {  /* only upper limit */
      int u = luaL_checkint(L, 1);
      luaL_argcheck(L, 1<=u, 1, "interval is empty");
      lua_pushnumber(L, (r % u)+1);  /* int between 1 and `u' */
      break;
    }
    case 2: {  /* lower and upper limits */
      int l = luaL_checkint(L, 1);
      int u = luaL_checkint(L, 2);
      luaL_argcheck(L, l<=u, 2, "interval is empty");
      lua_pushnumber(L, (r%(u-l+1))+l);  /* int between `l' and `u' */
      break;
    }
    default: return luaL_error(L, "wrong number of arguments");
  }
  return 1;
}

#else

static int math_random (lua_State *L) {
  /* the `%' avoids the (rare) case of r==1, and is needed also because on
     some systems (SunOS!) `rand()' may return a value larger than RAND_MAX */
  lua_Number r = (lua_Number)(rand()%RAND_MAX) / (lua_Number)RAND_MAX;
  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, r);  /* Number between 0 and 1 */
      break;
    }
    case 1: {  /* only upper limit */
      int u = luaL_checkint(L, 1);
      luaL_argcheck(L, 1<=u, 1, "interval is empty");
      lua_pushnumber(L, floor(r*u)+1);  /* int between 1 and `u' */
      break;
    }
    case 2: {  /* lower and upper limits */
      int l = luaL_checkint(L, 1);
      int u = luaL_checkint(L, 2);
      luaL_argcheck(L, l<=u, 2, "interval is empty");
      lua_pushnumber(L, floor(r*(u-l+1))+l);  /* int between `l' and `u' */
      break;
    }
    default: return luaL_error(L, "wrong number of arguments");
  }
  return 1;
}

#endif


static int math_randomseed (lua_State *L) {
  srand(luaL_checkint(L, 1));
  return 0;
}

LROT_PUBLIC_BEGIN(math)
#ifdef LUA_NUMBER_INTEGRAL
  LROT_FUNCENTRY( abs, math_abs )
  LROT_FUNCENTRY( ceil, math_identity )
  LROT_FUNCENTRY( floor, math_identity )
  LROT_FUNCENTRY( max, math_max )
  LROT_FUNCENTRY( min, math_min )
  LROT_FUNCENTRY( pow, math_pow )
  LROT_FUNCENTRY( random, math_random )
  LROT_FUNCENTRY( randomseed, math_randomseed )
  LROT_FUNCENTRY( sqrt, math_sqrt )
  LROT_NUMENTRY( huge, INT_MAX )
#else
  LROT_FUNCENTRY( abs, math_abs )
// LROT_FUNCENTRY( acos, math_acos )
// LROT_FUNCENTRY( asin, math_asin )
// LROT_FUNCENTRY( atan2, math_atan2 )
// LROT_FUNCENTRY( atan, math_atan )
  LROT_FUNCENTRY( ceil, math_ceil )
// LROT_FUNCENTRY( cosh, math_cosh )
// LROT_FUNCENTRY( cos, math_cos )
// LROT_FUNCENTRY( deg, math_deg )
// LROT_FUNCENTRY( exp, math_exp )
  LROT_FUNCENTRY( floor, math_floor )
// LROT_FUNCENTRY( fmod, math_fmod )
// LROT_FUNCENTRY( mod, math_fmod )
// LROT_FUNCENTRY( frexp, math_frexp )
// LROT_FUNCENTRY( ldexp, math_ldexp )
// LROT_FUNCENTRY( log10, math_log10 )
// LROT_FUNCENTRY( log, math_log )
  LROT_FUNCENTRY( max, math_max )
  LROT_FUNCENTRY( min, math_min )
// LROT_FUNCENTRY( modf, math_modf )
  LROT_FUNCENTRY( pow, math_pow )
// LROT_FUNCENTRY( rad, math_rad )
  LROT_FUNCENTRY( random, math_random )
  LROT_FUNCENTRY( randomseed, math_randomseed )
// LROT_FUNCENTRY( sinh, math_sinh )
// LROT_FUNCENTRY( sin, math_sin )
  LROT_FUNCENTRY( sqrt, math_sqrt )
// LROT_FUNCENTRY( tanh, math_tanh )
// LROT_FUNCENTRY( tan, math_tan )
  LROT_NUMENTRY( pi, PI )
  LROT_NUMENTRY( huge, HUGE_VAL )
#endif // #ifdef LUA_NUMBER_INTEGRAL
LROT_END(math, NULL, 0)


/*
** Open math library
*/
LUALIB_API int luaopen_math (lua_State *L) {
  (void)L;
  return 0;
}
