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
 */
#include "lextra.h"
#include "lauxlib.h"

bool luaL_optbool (lua_State *L, int idx, bool def)
{
  if (lua_isboolean (L, idx))
    return lua_toboolean (L, idx);
  else
    return def;
}


static int weak_mt_ref = LUA_NOREF;

int luaL_weak_ref(lua_State* L)
{
    lua_newtable(L); // new_table={}

    if (weak_mt_ref == LUA_NOREF) {
      lua_newtable(L); // metatable={}            
      lua_pushliteral(L, "__mode");
      lua_pushliteral(L, "v");
      lua_rawset(L, -3); // metatable._mode='v'
      lua_pushvalue(L, -1);
      weak_mt_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      lua_rawgeti(L, LUA_REGISTRYINDEX, weak_mt_ref);
    }
    lua_setmetatable(L, -2); // setmetatable(new_table,metatable)

    lua_pushvalue(L,-2); // push the previous top of stack
    lua_rawseti(L,-2,1); // new_table[1]=original value on top of the stack

    int ref = luaL_ref(L, LUA_REGISTRYINDEX); // this pops the new_table
    lua_pop(L,1); // pop the ref
    return ref;
}

void luaL_push_weak_ref(lua_State* L, int ref){
    if (ref <= 0){
        luaL_error(L, "invalid weak ref");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_rawgeti(L, -1, 1);
    lua_remove(L,-2);
}