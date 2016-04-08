#include "wifi_common.h"

void wifi_add_int_field(lua_State* L, char* name, lua_Integer integer)
{
  lua_pushinteger(L, integer);
  lua_setfield(L, -2,  name);
}
void wifi_add_sprintf_field(lua_State* L, char* name, char* string, ...)
{
  char buffer[256];
  va_list arglist;
  va_start( arglist, string );
  c_vsprintf( buffer, string, arglist );
  va_end( arglist );
  lua_pushstring(L, buffer);
  lua_setfield(L, -2,  name);
}
