#ifdef LUA_CROSS_COMPILER

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#define true  1
#define false 0

#else

#include <ctype.h>
#include <string.h>
#include <stdbool.h>

const char *lua_init_value = "@init.lua";

const char *c_getenv(const char *__string)
{
    if (strcmp(__string, "LUA_INIT") == 0)
    {
        return lua_init_value;
    }
    return NULL;
}

#endif
