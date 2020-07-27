// Helloworld sample module

#include "esp_log.h"
#include "lauxlib.h"
#include "lnodeaux.h"
#include "module.h"

static const char* HELLOWORLD_METATABLE = NODEMCU_MODULE_METATABLE();

// hello_context_t struct contains information to wrap a "hello world object"
typedef struct {
    char* my_name;  // pointer to the greeter's name
} hello_context_t;

// Lua: helloworldobj:hello(text)
static int helloworld_hello(lua_State* L) {
    hello_context_t* context = (hello_context_t*)luaL_checkudata(L, 1, HELLOWORLD_METATABLE);
    printf("Hello, %s: %s\n", context->my_name, luaL_optstring(L, 2, "How are you?"));
    return 0;
}

// helloworld_delete is called on garbage collection
static int helloworld_delete(lua_State* L) {
    hello_context_t* context = (hello_context_t*)luaL_checkudata(L, 1, HELLOWORLD_METATABLE);
    printf("Helloworld object with name '%s' garbage collected\n", context->my_name);
    luaX_free_string(L, context->my_name);
    return 0;
}

// Lua: modulename.new(string)
static int helloworld_new(lua_State* L) {
    //create a new lua userdata object and initialize to 0.
    hello_context_t* context = (hello_context_t*)lua_newuserdata(L, sizeof(hello_context_t));

    context->my_name = luaX_alloc_string(L, 1, 100);

    luaL_getmetatable(L, HELLOWORLD_METATABLE);
    lua_setmetatable(L, -2);

    return 1;  //one object returned, the helloworld context wrapped in a lua userdata object
}

// object function map:
LROT_BEGIN(helloworld_metatable)
LROT_FUNCENTRY(hello, helloworld_hello)
LROT_FUNCENTRY(__gc, helloworld_delete)
LROT_TABENTRY(__index, helloworld_metatable)
LROT_END(helloworld_metatable, NULL, 0)

// Module function map
LROT_BEGIN(module)
LROT_FUNCENTRY(new, helloworld_new)
LROT_END(module, NULL, 0)

// module_init is invoked on device startup
static int module_init(lua_State* L) {
    luaL_rometatable(L, HELLOWORLD_METATABLE, (void*)helloworld_metatable_map);  // create metatable for helloworld
    return 0;
}

NODEMCU_MODULE_STD();  // define Lua entries