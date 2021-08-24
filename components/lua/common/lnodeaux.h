/*
 * Copyright (c) 2019 the NodeMCU authors. All rights reserved
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
 * @author Javier Peletier <jm@epiclabs.io>
 * @brief This library exports useful functions to handle lua registry refs and strings
 */

#ifndef _NODEMCU_LNODEAUX_H_
#define _NODEMCU_LNODEAUX_H_

#include "lua.h"
#include <stdbool.h>

// lua_ref_t represents a reference to a lua object in the registry
typedef int lua_ref_t;

//luaX_weak_ref pops an item from the stack and returns a weak reference to it
lua_ref_t luaX_weak_ref(lua_State* L);

//luaL_push_weak takes a weak reference and pushes the original item on the stack
void luaX_push_weak_ref(lua_State* L, lua_ref_t ref);

// luaX_alloc_string creates a dynamically-allocated null-terminated string copying it
// from a lua stack position
char* luaX_alloc_string(lua_State* L, int idx, int max_length);

// luaX_free_string deallocates memory of a string allocated with luaX_alloc_string
void luaX_free_string(lua_State* L, char* st);

// luaX_unset_ref unpins a reference to a lua object in the registry
void luaX_unset_ref(lua_State* L, lua_ref_t* ref);

// luaX_set_ref pins a reference to a lua object, provided a registry
// or stack position
void luaX_set_ref(lua_State* L, int idx, lua_ref_t* ref);

// luaX_valid_ref returns true if the reference is set.
inline bool luaX_valid_ref(lua_ref_t ref) {
    return ref > 0;
}

// luaX_pushlstring is the same as lua_pushlstring except it will return nonzero in case of error
// (instead of throwing an exception and never returning) and will put the error message on the top of the stack.
int luaX_pushlstring(lua_State* L, const char* s, size_t l);
 

#endif
