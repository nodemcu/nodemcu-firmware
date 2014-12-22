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

/**
 * \file
 *         JSON output generation
 * \author
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */
#ifdef JSON_FORMAT
//#include "contiki.h"
#include "json/jsontree.h"
#include "json/jsonparse.h"
#include "osapi.h"
//#include <string.h>

#define DEBUG 0
#if DEBUG
//#include <stdio.h>
#define PRINTF(...) os_printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
void ICACHE_FLASH_ATTR
jsontree_write_atom(const struct jsontree_context *js_ctx, const char *text)
{
  if(text == NULL) {
    js_ctx->putchar('0');
  } else {
    while(*text != '\0') {
      js_ctx->putchar(*text++);
    }
  }
}
/*---------------------------------------------------------------------------*/
void ICACHE_FLASH_ATTR
jsontree_write_string(const struct jsontree_context *js_ctx, const char *text)
{
  js_ctx->putchar('"');
  if(text != NULL) {
    while(*text != '\0') {
      if(*text == '"') {
        js_ctx->putchar('\\');
      }
      js_ctx->putchar(*text++);
    }
  }
  js_ctx->putchar('"');
}
/*---------------------------------------------------------------------------*/
void ICACHE_FLASH_ATTR
jsontree_write_int(const struct jsontree_context *js_ctx, int value)
{
  char buf[10];
  int l;

  if(value < 0) {
    js_ctx->putchar('-');
    value = -value;
  }

  l = sizeof(buf) - 1;
  do {
    buf[l--] = '0' + (value % 10);
    value /= 10;
  } while(value > 0 && l >= 0);

  while(++l < sizeof(buf)) {
    js_ctx->putchar(buf[l]);
  }
}

/*---------------------------------------------------------------------------*/
void ICACHE_FLASH_ATTR
jsontree_write_int_array(const struct jsontree_context *js_ctx, const int *text, uint32 length)
{
  uint32 i = 0;
  if(text == NULL) {
    js_ctx->putchar('0');
  } else {
    for (i = 0; i < length - 1; i ++) {
      jsontree_write_int(js_ctx, *text++);
	  js_ctx->putchar(',');
    }
	jsontree_write_int(js_ctx, *text);
  }
}


/*---------------------------------------------------------------------------*/
void ICACHE_FLASH_ATTR
jsontree_setup(struct jsontree_context *js_ctx, struct jsontree_value *root,
               int (* putchar)(int))
{
  js_ctx->values[0] = root;
  js_ctx->putchar = putchar;
  js_ctx->path = 0;
  jsontree_reset(js_ctx);
}
/*---------------------------------------------------------------------------*/
void ICACHE_FLASH_ATTR
jsontree_reset(struct jsontree_context *js_ctx)
{
  js_ctx->depth = 0;
  js_ctx->index[0] = 0;
}
/*---------------------------------------------------------------------------*/
const char *ICACHE_FLASH_ATTR
jsontree_path_name(const struct jsontree_context *js_ctx, int depth)
{
  if(depth < js_ctx->depth && js_ctx->values[depth]->type == JSON_TYPE_OBJECT) {
    return ((struct jsontree_object *)js_ctx->values[depth])->
      pairs[js_ctx->index[depth]].name;
  }
  return "";
}
/*---------------------------------------------------------------------------*/
int ICACHE_FLASH_ATTR
jsontree_print_next(struct jsontree_context *js_ctx)
{
  struct jsontree_value *v;
  int index;

  v = js_ctx->values[js_ctx->depth];

  /* Default operation after switch is to back up one level */
  switch(v->type) {
  case JSON_TYPE_OBJECT:
  case JSON_TYPE_ARRAY: {
    struct jsontree_array *o = (struct jsontree_array *)v;
    struct jsontree_value *ov;

    index = js_ctx->index[js_ctx->depth];
    if(index == 0) {
      js_ctx->putchar(v->type);
      js_ctx->putchar('\n');
    }
    if(index >= o->count) {
      js_ctx->putchar('\n');
      js_ctx->putchar(v->type + 2);
      /* Default operation: back up one level! */
      break;
    }

    if(index > 0) {
      js_ctx->putchar(',');
      js_ctx->putchar('\n');
    }
    if(v->type == JSON_TYPE_OBJECT) {
      jsontree_write_string(js_ctx,
                            ((struct jsontree_object *)o)->pairs[index].name);
      js_ctx->putchar(':');
      ov = ((struct jsontree_object *)o)->pairs[index].value;
    } else {
      ov = o->values[index];
    }
    /* TODO check max depth */
    js_ctx->depth++;          /* step down to value... */
    js_ctx->index[js_ctx->depth] = 0; /* and init index */
    js_ctx->values[js_ctx->depth] = ov;
    /* Continue on this new level */
    return 1;
  }
  case JSON_TYPE_STRING:
    jsontree_write_string(js_ctx, ((struct jsontree_string *)v)->value);
    /* Default operation: back up one level! */
    break;
  case JSON_TYPE_INT:
    jsontree_write_int(js_ctx, ((struct jsontree_int *)v)->value);
    /* Default operation: back up one level! */
    break;
  case JSON_TYPE_CALLBACK: {   /* pre-formatted json string currently */
    struct jsontree_callback *callback;

    callback = (struct jsontree_callback *)v;
    if(js_ctx->index[js_ctx->depth] == 0) {
      /* First call: reset the callback status */
      js_ctx->callback_state = 0;
    }
    if(callback->output == NULL) {
      jsontree_write_string(js_ctx, "");
    } else if(callback->output(js_ctx)) {
      /* The callback wants to output more */
      js_ctx->index[js_ctx->depth]++;
      return 1;
    }
    /* Default operation: back up one level! */
    break;
  }
  default:
    PRINTF("\nError: Illegal json type:'%c'\n", v->type);
    return 0;
  }
  /* Done => back up one level! */
  if(js_ctx->depth > 0) {
    js_ctx->depth--;
    js_ctx->index[js_ctx->depth]++;
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static struct jsontree_value *ICACHE_FLASH_ATTR
find_next(struct jsontree_context *js_ctx)
{
  struct jsontree_value *v;
  int index;

  do {
    v = js_ctx->values[js_ctx->depth];

    /* Default operation after switch is to back up one level */
    switch(v->type) {
    case JSON_TYPE_OBJECT:
    case JSON_TYPE_ARRAY: {
      struct jsontree_array *o = (struct jsontree_array *)v;
      struct jsontree_value *ov;

      index = js_ctx->index[js_ctx->depth];
      if(index >= o->count) {
        /* Default operation: back up one level! */
        break;
      }

      if(v->type == JSON_TYPE_OBJECT) {
        ov = ((struct jsontree_object *)o)->pairs[index].value;
      } else {
        ov = o->values[index];
      }
      /* TODO check max depth */
      js_ctx->depth++;        /* step down to value... */
      js_ctx->index[js_ctx->depth] = 0;       /* and init index */
      js_ctx->values[js_ctx->depth] = ov;
      /* Continue on this new level */
      return ov;
    }
    default:
      /* Default operation: back up one level! */
      break;
    }
    /* Done => back up one level! */
    if(js_ctx->depth > 0) {
      js_ctx->depth--;
      js_ctx->index[js_ctx->depth]++;
    } else {
      return NULL;
    }
  } while(1);
}
/*---------------------------------------------------------------------------*/
struct jsontree_value *ICACHE_FLASH_ATTR
jsontree_find_next(struct jsontree_context *js_ctx, int type)
{
  struct jsontree_value *v;

  while((v = find_next(js_ctx)) != NULL && v->type != type &&
        js_ctx->path < js_ctx->depth) {
    /* search */
  }
  js_ctx->callback_state = 0;
  return js_ctx->path < js_ctx->depth ? v : NULL;
}
/*---------------------------------------------------------------------------*/
#endif

