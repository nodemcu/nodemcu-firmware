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

#ifndef _NODEMCU_LEXTRA_H_
#define _NODEMCU_LEXTRA_H_

#include <stdbool.h>
#include "lua.h"

// lua_ref_t represents a reference to a lua object in the registry
typedef int lua_ref_t;

bool luaL_optbool(lua_State* L, int idx, bool def);

//luaL_weak_ref pops an item from the stack and returns a weak reference to it
lua_ref_t luaL_weak_ref(lua_State* L);

//luaL_push_weak takes a weak reference and pushes the original item on the stack
void luaL_push_weak_ref(lua_State* L, lua_ref_t ref);

// alloc_string creates a dynamically-allocated string copying it
// from a lua stack position
char* alloc_string(lua_State* L, int idx, int max_length);

// free_string deallocates memory of a string allocated with alloc_string
void free_string(lua_State* L, char* st);

// unset_ref unpins a reference to a lua object in the registry
void unset_ref(lua_State* L, lua_ref_t* ref);

// set_ref pins a reference to a lua object, provided a registry
// or stack position
void set_ref(lua_State* L, int idx, lua_ref_t* ref);

#endif
