#include "module.h"

static int lmymod_hello(lua_State *L)
{
  if (lua_isnoneornil(L, 1))
    lua_pushliteral(L, "world");

  lua_getglobal(L, "print");
  lua_pushliteral(L, "Hello,");
  lua_pushvalue(L, 1);
  lua_call(L, 2, 0);

  return 0;
}


LROT_BEGIN(mymod, NULL, 0)
  LROT_FUNCENTRY(hello,   lmymod_hello)
LROT_END(mymod, NULL, 0)

NODEMCU_MODULE(MYMOD, "mymod", mymod, NULL);
