/*
 * Copyright (c) 2011-2012, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

#ifdef JSON_FORMAT

#include "json/jsonparse.h"
#include "osapi.h"
//#include <stdlib.h>
//#include <string.h>

/*--------------------------------------------------------------------*/
static int ICACHE_FLASH_ATTR
push(struct jsonparse_state *state, char c)
{
  state->stack[state->depth] = c;
  state->depth++;
  state->vtype = 0;
  return state->depth < JSONPARSE_MAX_DEPTH;
}
/*--------------------------------------------------------------------*/
static char ICACHE_FLASH_ATTR
pop(struct jsonparse_state *state)
{
  if(state->depth == 0) {
    return JSON_TYPE_ERROR;
  }
  state->depth--;
  return state->stack[state->depth];
}
/*--------------------------------------------------------------------*/
/* will pass by the value and store the start and length of the value for
   atomic types */
/*--------------------------------------------------------------------*/
static void ICACHE_FLASH_ATTR
atomic(struct jsonparse_state *state, char type)
{
  char c;

  state->vstart = state->pos;
  state->vtype = type;
  if(type == JSON_TYPE_STRING || type == JSON_TYPE_PAIR_NAME) {
    while((c = state->json[state->pos++]) && c != '"') {
      if(c == '\\') {
        state->pos++;           /* skip current char */
      }
    }
    state->vlen = state->pos - state->vstart - 1;
  } else if(type == JSON_TYPE_NUMBER) {
    do {
      c = state->json[state->pos];
      if((c < '0' || c > '9') && c != '.') {
        c = 0;
      } else {
        state->pos++;
      }
    } while(c);
    /* need to back one step since first char is already gone */
    state->vstart--;
    state->vlen = state->pos - state->vstart;
  }
  /* no other types for now... */
}
/*--------------------------------------------------------------------*/
static void ICACHE_FLASH_ATTR
skip_ws(struct jsonparse_state *state)
{
  char c;

  while(state->pos < state->len &&
        ((c = state->json[state->pos]) == ' ' || c == '\n')) {
    state->pos++;
  }
}
/*--------------------------------------------------------------------*/
void ICACHE_FLASH_ATTR
jsonparse_setup(struct jsonparse_state *state, const char *json, int len)
{
  state->json = json;
  state->len = len;
  state->pos = 0;
  state->depth = 0;
  state->error = 0;
  state->stack[0] = 0;
}
/*--------------------------------------------------------------------*/
int ICACHE_FLASH_ATTR
jsonparse_next(struct jsonparse_state *state)
{
  char c;
  char s;

  skip_ws(state);
  c = state->json[state->pos];
  s = jsonparse_get_type(state);
  state->pos++;

  switch(c) {
  case '{':
    push(state, c);
    return c;
  case '}':
    if(s == ':' && state->vtype != 0) {
/*       printf("Popping vtype: '%c'\n", state->vtype); */
      pop(state);
      s = jsonparse_get_type(state);
    }
    if(s == '{') {
      pop(state);
    } else {
      state->error = JSON_ERROR_SYNTAX;
      return JSON_TYPE_ERROR;
    }
    return c;
  case ']':
    if(s == '[') {
      pop(state);
    } else {
      state->error = JSON_ERROR_UNEXPECTED_END_OF_ARRAY;
      return JSON_TYPE_ERROR;
    }
    return c;
  case ':':
    push(state, c);
    return c;
  case ',':
    /* if x:y ... , */
    if(s == ':' && state->vtype != 0) {
      pop(state);
    } else if(s == '[') {
      /* ok! */
    } else {
      state->error = JSON_ERROR_SYNTAX;
      return JSON_TYPE_ERROR;
    }
    return c;
  case '"':
    if(s == '{' || s == '[' || s == ':') {
      atomic(state, c = (s == '{' ? JSON_TYPE_PAIR_NAME : c));
    } else {
      state->error = JSON_ERROR_UNEXPECTED_STRING;
      return JSON_TYPE_ERROR;
    }
    return c;
  case '[':
    if(s == '{' || s == '[' || s == ':') {
      push(state, c);
    } else {
      state->error = JSON_ERROR_UNEXPECTED_ARRAY;
      return JSON_TYPE_ERROR;
    }
    return c;
  default:
    if(s == ':' || s == '[') {
      if(c <= '9' && c >= '0') {
        atomic(state, JSON_TYPE_NUMBER);
        return JSON_TYPE_NUMBER;
      }
    }
  }
  return 0;
}
/*--------------------------------------------------------------------*/
/* get the json value of the current position
 * works only on "atomic" values such as string, number, null, false, true
 */
/*--------------------------------------------------------------------*/
int ICACHE_FLASH_ATTR
jsonparse_copy_value(struct jsonparse_state *state, char *str, int size)
{
  int i;
  char z = 0;
  char y = 0;

  if(state->vtype == 0) {
    return 0;
  }
  size = size <= state->vlen ? (size - 1) : state->vlen;
  for(i = 0; i < size; i++) {
    if (y == 0 && state->json[state->vstart + i] == '\\') {
        y = 1;
        z++;
        continue;
    }
    y = 0;
    str[i - z] = state->json[state->vstart + i];
  }
  str[i - z] = 0;
  return state->vtype;
}
/*--------------------------------------------------------------------*/
int ICACHE_FLASH_ATTR
jsonparse_get_value_as_int(struct jsonparse_state *state)
{
  if(state->vtype != JSON_TYPE_NUMBER) {
    return 0;
  }
  return atoi(&state->json[state->vstart]);
}
/*--------------------------------------------------------------------*/
long ICACHE_FLASH_ATTR
jsonparse_get_value_as_long(struct jsonparse_state *state)
{
  if(state->vtype != JSON_TYPE_NUMBER) {
    return 0;
  }
  return atol(&state->json[state->vstart]);
}

/*--------------------------------------------------------------------*/
unsigned long ICACHE_FLASH_ATTR
jsonparse_get_value_as_ulong(struct jsonparse_state *state)
{
  if(state->vtype != JSON_TYPE_NUMBER) {
    return 0;
  }
  return strtoul(&state->json[state->vstart], '\0', 0);
}

/*--------------------------------------------------------------------*/
/* strcmp - assume no strange chars that needs to be stuffed in string... */
/*--------------------------------------------------------------------*/
int ICACHE_FLASH_ATTR
jsonparse_strcmp_value(struct jsonparse_state *state, const char *str)
{
  if(state->vtype == 0) {
    return -1;
  }
  return os_strncmp(str, &state->json[state->vstart], state->vlen);
}
/*--------------------------------------------------------------------*/
int ICACHE_FLASH_ATTR
jsonparse_get_len(struct jsonparse_state *state)
{
  return state->vlen;
}
/*--------------------------------------------------------------------*/
int ICACHE_FLASH_ATTR
jsonparse_get_type(struct jsonparse_state *state)
{
  if(state->depth == 0) {
    return 0;
  }
  return state->stack[state->depth - 1];
}
/*--------------------------------------------------------------------*/
int ICACHE_FLASH_ATTR
jsonparse_has_next(struct jsonparse_state *state)
{
  return state->pos < state->len;
}
/*--------------------------------------------------------------------*/
#endif

