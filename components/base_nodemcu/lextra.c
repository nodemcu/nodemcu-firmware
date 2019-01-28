/*
 * Copyright 2016 Dius Computing Pty Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 * @author Javier Peletier <jm@epiclabs.io>
 */
#include "lextra.h"
#include <string.h>
#include "lauxlib.h"
#include "lmem.h"

bool luaL_optbool(lua_State* L, int idx, bool def) {
    if (lua_isboolean(L, idx))
        return lua_toboolean(L, idx);
    else
        return def;
}

// reference to the weak references metatable
static lua_ref_t weak_mt_ref = LUA_NOREF;

// luaL_weak_ref pops an item from the stack and returns a weak reference to it
// inspired by https://stackoverflow.com/a/19340846
lua_ref_t luaL_weak_ref(lua_State* L) {
    lua_newtable(L);  // push a table on the stack that will serve as proxy of our item

    // Initialize weak metatable if this is the first call
    if (weak_mt_ref == LUA_NOREF) {
        lua_newtable(L);  // push a new table on the stack to serve as metatable
        lua_pushliteral(L, "__mode");
        lua_pushliteral(L, "v");
        lua_rawset(L, -3);                             // metatable._mode='v' (values are weak) http://lua-users.org/wiki/WeakTablesTutorial
        lua_pushvalue(L, -1);                          // duplicate metatable on the stack
        weak_mt_ref = luaL_ref(L, LUA_REGISTRYINDEX);  // store ref to weak metatable in the registry (pops 1 item)
    } else {
        lua_rawgeti(L, LUA_REGISTRYINDEX, weak_mt_ref);  // retrieve metatable from registry
    }

    lua_setmetatable(L, -2);  // setmetatable(proxy,metatable)

    lua_pushvalue(L, -2);   // push the previous top of stack
    lua_rawseti(L, -2, 1);  // proxy[1]=original value on top of the stack

    lua_ref_t ref = luaL_ref(L, LUA_REGISTRYINDEX);  // this pops the proxy
    lua_pop(L, 1);                                   // pop the ref
    return ref;
}

//luaL_push_weak takes a weak reference and pushes the original item on the stack
void luaL_push_weak_ref(lua_State* L, lua_ref_t ref) {
    if (ref <= 0) {
        luaL_error(L, "invalid weak ref");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);  // push the proxy object on the stack
    lua_rawgeti(L, -1, 1);                   // push proxy[1] (our proxied item) on the stack
    lua_remove(L, -2);                       // remove proxy from underneath the stack.
    // Retrieved item remains on top, as output of this function.
}

// alloc_string creates a dynamically-allocated string copying it
// from a lua stack position
char* alloc_string(lua_State* L, int idx, int max_length) {
    const char* lua_st = luaL_checkstring(L, idx);  //retrieve string from lua
                                                    // measure the string and limit it to max_length
    int len = strlen(lua_st);
    if (len > max_length)
        len = max_length;

    // allocate memory for our copy, saving 1 byte to null-terminate it.
    char* st = luaM_malloc(L, len + 1);

    // actually make a copy
    strncpy(st, lua_st, len);

    // terminate it with a null char.
    st[len] = '\0';
    return st;
}

// free_string deallocates memory of a string allocated with alloc_string
void free_string(lua_State* L, char* st) {
    if (st)
        luaM_freearray(L, st, strlen(st) + 1, char);
}

// unset_ref unpins a reference to a lua object in the registry
void unset_ref(lua_State* L, lua_ref_t* ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    *ref = LUA_NOREF;
}

// set_ref pins a reference to a lua object, provided a registry
// or stack position
void set_ref(lua_State* L, int idx, lua_ref_t* ref) {
    unset_ref(L, ref);                      // make sure we free previous reference
    lua_pushvalue(L, idx);                  // push on the stack the referenced index
    *ref = luaL_ref(L, LUA_REGISTRYINDEX);  // set the reference (pops 1 value)
}
