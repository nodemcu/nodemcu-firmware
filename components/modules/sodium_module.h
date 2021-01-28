#ifndef sodium_module_h
#define sodium_module_h

typedef struct lua_State lua_State;

int l_sodium_generichash(lua_State* L);
int l_sodium_generichash_init(lua_State* L);

#endif
