#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lstring.h"

#ifndef LOCAL_LUA
#include "module.h"
#include "c_string.h"
#include "c_math.h"
#include "c_limits.h"
#endif

#include "json_config.h"
#include "jsonsl.h"

#define LUA_SJSONLIBNAME "sjson"

#define DEFAULT_DEPTH   20

#define DBG_PRINTF(...)    

typedef struct {
  jsonsl_t jsn;
  int result_ref;
  int hkey_ref;
  int null_ref;
  int metatable;
  int pos_ref;
  uint8_t complete;
  const char *error;
  lua_State *L;
  size_t min_needed;
  size_t min_available;
  size_t buffer_len;
  const char *buffer; // Points into buffer_ref
  int buffer_ref;
} JSN_DATA;

#define get_parent_object_ref() ((state->level == 1) ? data->result_ref : state[-1].lua_object_ref)
#define get_parent_object_used_count_pre_inc() ((state->level == 1) ? 1 : ++state[-1].used_count)

static const char* get_state_buffer(JSN_DATA *ctx, struct jsonsl_state_st *state)
{
  size_t offset = state->pos_begin - ctx->min_available;
  return ctx->buffer + offset;
}

// The elem data is a ref

static int error_callback(jsonsl_t jsn,
                   jsonsl_error_t err,
                   struct jsonsl_state_st *state,
                   char *at)
{
  JSN_DATA *data = (JSN_DATA *) jsn->data;
  if (!data->complete) {
    data->error = jsonsl_strerror(err);
  }

  //fprintf(stderr, "Got error at pos %lu: %s\n", jsn->pos, jsonsl_strerror(err));
  return 0;
}

static void
create_table(JSN_DATA *data) {
  lua_newtable(data->L);
  if (data->metatable != LUA_NOREF) {
    lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->metatable);
    lua_setmetatable(data->L, -2);
  }
}

static void
create_new_element(jsonsl_t jsn,
                   jsonsl_action_t action,
                   struct jsonsl_state_st *state,
                   const char *buf)
{
  JSN_DATA *data = jsn->data;

  DBG_PRINTF("L%d: new action %d @ %d state->type %s\n", state->level, action, state->pos_begin, jsonsl_strtype(state->type));
  DBG_PRINTF("buf: '%s' ('%.10s')\n", buf, get_state_buffer(data, state));

  state->lua_object_ref = LUA_NOREF;

  switch(state->type) {
    case JSONSL_T_SPECIAL:
    case JSONSL_T_STRING: 
    case JSONSL_T_HKEY: 
      break;

    case JSONSL_T_LIST: 
    case JSONSL_T_OBJECT: 
      create_table(data);
      state->lua_object_ref = lua_ref(data->L, 1);
      state->used_count = 0;

      lua_rawgeti(data->L, LUA_REGISTRYINDEX, get_parent_object_ref());
      if (data->hkey_ref == LUA_NOREF) {
        // list, so append
        lua_pushnumber(data->L, get_parent_object_used_count_pre_inc());
        DBG_PRINTF("Adding array element\n");
      } else {
        // object, so 
        lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->hkey_ref);
        lua_unref(data->L, data->hkey_ref);
        data->hkey_ref = LUA_NOREF;
        DBG_PRINTF("Adding hash element\n");
      }
      if (data->pos_ref != LUA_NOREF && state->level > 1) {
        lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->pos_ref);
        lua_pushnumber(data->L, state->level - 1);
        lua_pushvalue(data->L, -3);     // get the key
        lua_settable(data->L, -3);
        lua_pop(data->L, 1);
      }
      // At this point, the stack:
      // top: index/hash key
      //    : table
      
      int want_value = 1;
      // Invoke the checkpath method if possible
      if (data->pos_ref != LUA_NOREF) {
        lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->metatable);
        lua_getfield(data->L, -1, "checkpath");
        if (lua_type(data->L, -1) != LUA_TNIL) {
          // Call with the new table and the path as arguments
          lua_rawgeti(data->L, LUA_REGISTRYINDEX, state->lua_object_ref);
          lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->pos_ref);
          lua_call(data->L, 2, 1);
          want_value = lua_toboolean(data->L, -1);
        }
        lua_pop(data->L, 2);    // Discard the metatable and either the getfield result or retval
      }

      if (want_value) {
        lua_rawgeti(data->L, LUA_REGISTRYINDEX, state->lua_object_ref);
        lua_settable(data->L, -3);
        lua_pop(data->L, 1);    // the table
      } else {
        lua_pop(data->L, 2);    // the index and table
      }

      break;

    default:
        DBG_PRINTF("Unhandled type %c\n", state->type);
        luaL_error(data->L, "Unhandled type");
        break;
    }

    data->min_needed = state->pos_begin;
}

static void push_number(JSN_DATA *data, struct jsonsl_state_st *state) {
  lua_pushlstring(data->L, get_state_buffer(data, state), state->pos_cur - state->pos_begin);
  LUA_NUMBER r = lua_tonumber(data->L, -1);
  lua_pop(data->L, 1);
  lua_pushnumber(data->L, r);
}

static int fromhex(char c) {
  if (c <= '9') {
    return c & 0xf;
  }

  return ((c - 'A' + 10) & 0xf);
}

static void output_utf8(luaL_Buffer *buf, int c) {
  char space[4];
  char *b = space;

  if (c<0x80) *b++=c;
  else if (c<0x800) *b++=192+c/64, *b++=128+c%64;
  else if (c-0xd800u<0x800) *b++ = '?';
  else if (c<0x10000) *b++=224+c/4096, *b++=128+c/64%64, *b++=128+c%64;
  else if (c<0x110000) *b++=240+c/262144, *b++=128+c/4096%64, *b++=128+c/64%64, *b++=128+c%64;
  else *b++ = '?';

  luaL_addlstring(buf, space, b - space);
}

static void push_string(JSN_DATA *data, struct jsonsl_state_st *state) {
  luaL_Buffer b;
  luaL_buffinit(data->L, &b);
  int i;
  const char *c = get_state_buffer(data, state) + 1;
  for (i = 0; i < state->pos_cur - state->pos_begin - 1; i++) {
    int nc = c[i];
    if (nc == '\\') {
      i++;
      nc = c[i] & 255;
      switch (c[i]) {
        case 'b':
          nc = '\b';
          break;
        case 'f':
          nc = '\f';
          break;
        case 'n':
          nc = '\n';
          break;
        case 'r':
          nc = '\r';
          break;
        case 't':
          nc = '\t';
          break;
        case 'u':
          nc = fromhex(c[++i]) << 12;
          nc += fromhex(c[++i]) << 8;
          nc += fromhex(c[++i]) << 4;
          nc += fromhex(c[++i])     ;
          output_utf8(&b, nc);
          continue;
      }
    }
    luaL_putchar(&b, nc);
  }
  luaL_pushresult(&b);
}

static void
cleanup_closing_element(jsonsl_t jsn,
                        jsonsl_action_t action,
                        struct jsonsl_state_st *state,
                        const char *at)
{
  JSN_DATA *data = (JSN_DATA *) jsn->data;
  DBG_PRINTF( "L%d: cc action %d state->type %s\n", state->level, action, jsonsl_strtype(state->type));
  DBG_PRINTF( "buf (%d - %d): '%.*s'\n", state->pos_begin, state->pos_cur, state->pos_cur - state->pos_begin, get_state_buffer(data, state));
  DBG_PRINTF( "at: '%s'\n", at);

 switch (state->type) {
   case JSONSL_T_HKEY:
      push_string(data, state);
      data->hkey_ref = lua_ref(data->L, 1);
      break;

   case JSONSL_T_STRING:
      lua_rawgeti(data->L, LUA_REGISTRYINDEX, get_parent_object_ref());
      if (data->hkey_ref == LUA_NOREF) {
        // list, so append
        lua_pushnumber(data->L, get_parent_object_used_count_pre_inc());
      } else {
        // object, so 
        lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->hkey_ref);
        lua_unref(data->L, data->hkey_ref);
        data->hkey_ref = LUA_NOREF;
      }
      push_string(data, state);
      lua_settable(data->L, -3);
      lua_pop(data->L, 1);
      break;

   case JSONSL_T_SPECIAL:
      DBG_PRINTF("Special flags = 0x%x\n", state->special_flags);
      // need to deal with true/false/null

      if (state->special_flags & (JSONSL_SPECIALf_TRUE|JSONSL_SPECIALf_FALSE|JSONSL_SPECIALf_NUMERIC|JSONSL_SPECIALf_NULL)) {
        if (state->special_flags & JSONSL_SPECIALf_TRUE) {
          lua_pushboolean(data->L, 1);
        } else if (state->special_flags & JSONSL_SPECIALf_FALSE) {
          lua_pushboolean(data->L, 0);
        } else if (state->special_flags & JSONSL_SPECIALf_NULL) {
          DBG_PRINTF("Outputting null\n");
          lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->null_ref);
        } else if (state->special_flags & JSONSL_SPECIALf_NUMERIC) {
          push_number(data, state);
        }

        lua_rawgeti(data->L, LUA_REGISTRYINDEX, get_parent_object_ref());
        if (data->hkey_ref == LUA_NOREF) {
          // list, so append
          lua_pushnumber(data->L, get_parent_object_used_count_pre_inc());
        } else {
          // object, so 
          lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->hkey_ref);
          lua_unref(data->L, data->hkey_ref);
          data->hkey_ref = LUA_NOREF;
        }
        lua_pushvalue(data->L, -3);
        lua_remove(data->L, -4);
        lua_settable(data->L, -3);
        lua_pop(data->L, 1);
      }
      break;
   case JSONSL_T_OBJECT:
   case JSONSL_T_LIST:
      lua_unref(data->L, state->lua_object_ref);
      state->lua_object_ref = LUA_NOREF;
      if (data->pos_ref != LUA_NOREF) {
        lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->pos_ref);
        lua_pushnumber(data->L, state->level);
        lua_pushnil(data->L);
        lua_settable(data->L, -3);
        lua_pop(data->L, 1);
      }
      if (state->level == 1) {
        data->complete = 1;
      }
      break;
 }
}

static int sjson_decoder_int(lua_State *L, int argno) {
  int nlevels = DEFAULT_DEPTH;

  if (lua_type(L, argno) == LUA_TTABLE) {
    lua_getfield(L, argno, "depth");
    nlevels = lua_tointeger(L, argno);
    if (nlevels == 0) {
      nlevels = DEFAULT_DEPTH;
    }
    if (nlevels < 4) {
      nlevels = 4;
    }
    if (nlevels > 1000) {
      nlevels = 1000;
    }
    lua_pop(L, 1);
  }

  JSN_DATA *data = (JSN_DATA *) lua_newuserdata(L, sizeof(JSN_DATA) + jsonsl_get_size(nlevels));
  //
  // Associate its metatable
  luaL_getmetatable(L, "sjson.decoder");
  lua_setmetatable(L, -2);

  jsonsl_t jsn = jsonsl_init((jsonsl_t) (data + 1), nlevels);
  int i;

  for (i = 0; i < jsn->levels_max; i++) {
    jsn->stack[i].lua_object_ref = LUA_NOREF;
  }
  data->jsn = jsn;
  data->result_ref = LUA_NOREF;

  data->null_ref = LUA_REFNIL;
  data->metatable = LUA_NOREF;
  data->hkey_ref = LUA_NOREF;
  data->pos_ref = LUA_NOREF;
  data->buffer_ref = LUA_NOREF;
  data->complete = 0;
  data->error = NULL;
  data->L = L;
  data->buffer_len = 0;
  
  data->min_needed = data->min_available = jsn->pos;

  lua_pushlightuserdata(L, 0);
  data->null_ref = lua_ref(L, 1);

  // This may throw...
  lua_newtable(L);
  data->result_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  if (lua_type(L, argno) == LUA_TTABLE) {
    luaL_unref(L, LUA_REGISTRYINDEX, data->null_ref);
    data->null_ref = LUA_NOREF;
    lua_getfield(L, argno, "null");
    data->null_ref = lua_ref(L, 1);

    lua_getfield(L, argno, "metatable");
    lua_pushvalue(L, -1);
    data->metatable = lua_ref(L, 1);

    if (lua_type(L, -1) != LUA_TNIL) {
      lua_getfield(L, -1, "checkpath");
      if (lua_type(L, -1) != LUA_TNIL) {
        lua_newtable(L);
        data->pos_ref = lua_ref(L, 1);
      }
      lua_pop(L, 1);      // Throw away the checkpath value 
    }
    lua_pop(L, 1);      // Throw away the metatable
  }

  jsonsl_enable_all_callbacks(data->jsn);
  
  jsn->action_callback = NULL;
  jsn->action_callback_PUSH = create_new_element;
  jsn->action_callback_POP = cleanup_closing_element;
  jsn->error_callback = error_callback;
  jsn->data = data;
  jsn->max_callback_level = nlevels;

  return 1;
}

static int sjson_decoder(lua_State *L) {
  return sjson_decoder_int(L, 1);
}

static int sjson_decoder_result_int(lua_State *L, JSN_DATA *data) {
  if (!data->complete) {
    luaL_error(L, "decode not complete");
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, data->result_ref);
  lua_rawgeti(L, -1, 1);
  lua_remove(L, -2);

  return 1;
}

static int sjson_decoder_result(lua_State *L) {
  JSN_DATA *data = (JSN_DATA *)luaL_checkudata(L, 1, "sjson.decoder");

  return sjson_decoder_result_int(L, data);
}


static void sjson_free_working_data(lua_State *L, JSN_DATA *data) {
  jsonsl_t jsn = data->jsn;
  int i;

  for (i = 0; i < jsn->levels_max; i++) {
    luaL_unref(L, LUA_REGISTRYINDEX, jsn->stack[i].lua_object_ref);
    jsn->stack[i].lua_object_ref = LUA_NOREF;
  }

  luaL_unref(L, LUA_REGISTRYINDEX, data->metatable);
  data->metatable = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, data->hkey_ref);
  data->hkey_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, data->null_ref);
  data->null_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, data->pos_ref);
  data->pos_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, data->buffer_ref);
  data->buffer_ref = LUA_NOREF;
}

static int sjson_decoder_write_int(lua_State *L, int udata_pos, int string_pos) {
  JSN_DATA *data = (JSN_DATA *)luaL_checkudata(L, udata_pos, "sjson.decoder");
  size_t len;

  const char *str = luaL_checklstring(L, string_pos, &len);

  if (data->error) {
    luaL_error(L, "JSON parse error: previous call");
  }

  if (!data->complete) {
    data->L = L;

    // Merge into any existing buffer and deal with discard
    if (data->buffer_ref != LUA_NOREF) {
      luaL_Buffer b;
      luaL_buffinit(L, &b);

      lua_rawgeti(L, LUA_REGISTRYINDEX, data->buffer_ref);
      size_t prev_len;
      const char *prev_buffer = luaL_checklstring(L, -1, &prev_len);
      lua_pop(L, 1);              // But string still referenced so it cannot move
      int discard = data->min_needed - data->min_available;
      prev_buffer += discard;
      prev_len -= discard;
      if (prev_len > 0) {
        luaL_addlstring(&b, prev_buffer, prev_len);
      }
      data->min_available += discard;

      luaL_unref(L, LUA_REGISTRYINDEX, data->buffer_ref);
      data->buffer_ref = LUA_NOREF;

      lua_pushvalue(L, string_pos);
      luaL_addvalue(&b);
      luaL_pushresult(&b);
    } else {
      lua_pushvalue(L, string_pos);
    }

    size_t blen;
    data->buffer = luaL_checklstring(L, -1, &blen);
    data->buffer_len = blen;
    data->buffer_ref = lua_ref(L, 1);

    jsonsl_feed(data->jsn, str, len);

    if (data->error) {
      luaL_error(L, "JSON parse error: %s", data->error);
    }
  }

  if (data->complete) {
    // We no longer need the buffer
    sjson_free_working_data(L, data);

    return sjson_decoder_result_int(L, data);
  }

  return 0;
}

static int sjson_decoder_write(lua_State *L) {
  return sjson_decoder_write_int(L, 1, 2);
}

static int sjson_decode(lua_State *L) {
  int push_count = sjson_decoder_int(L, 2);
  if (push_count != 1) {
    luaL_error(L, "Internal error in sjson.deocder");
  }

  luaL_checkudata(L, -1, "sjson.decoder");

  push_count = sjson_decoder_write_int(L, -1, 1);

  if (push_count != 1) {
    luaL_error(L, "Incomplete JSON object passed to sjson.decode");
  }

  // Now we have two items on the stack -- the udata and the result
  lua_remove(L, -2);

  return 1;
}

static int sjson_decoder_destructor(lua_State *L) {
  JSN_DATA *data = (JSN_DATA *)luaL_checkudata(L, 1, "sjson.decoder");

  sjson_free_working_data(L, data);

  data->jsn = NULL;

  luaL_unref(L, LUA_REGISTRYINDEX, data->result_ref);
  data->result_ref = LUA_NOREF;

  DBG_PRINTF("Destructor called\n");

  return 0;
}
//
//--------------------------------- ENCODER BELOW
//
//
//
//#undef DBG_PRINTF
//#define DBG_PRINTF printf
typedef struct {
  int lua_object_ref;
  // for arrays
  // 0 -> [
  // 1 -> first element
  // 2 -> ,
  // 3 -> second element
  // 4 -> ]
  // for objects
  // 0 -> { firstkey :
  // 1 -> first value
  // 2 -> , secondkey :
  // 3 -> second value
  // 4 -> }
  short offset;
  // -1 for objects
  // 0 -> n maximum integer key = n
  short size;
  int lua_key_ref;
} ENC_DATA_STATE;

typedef struct {
  ENC_DATA_STATE *stack;
  int nlevels;
  int level;
  int current_str_ref;
  int null_ref;
  int offset;

} ENC_DATA;

static int sjson_encoder_get_table_size(lua_State *L, int argno) {
  // Returns -1 for object, otherwise the maximum integer key value found.
  lua_pushvalue(L, argno);
  // stack now contains: -1 => table
  lua_pushnil(L);
  // stack now contains: -1 => nil; -2 => table
  //
  int maxkey = 0;

  while (lua_next(L, -2)) {
    lua_pop(L, 1);
    // stack now contains: -1 => key; -2 => table
    if (lua_type(L, -1) == LUA_TNUMBER) {
      int val = lua_tointeger(L, -1);
      if (val > maxkey) {
        maxkey = val;
      } else if (val <= 0) {
        maxkey = -1;
        lua_pop(L, 1);
        break;
      }
    } else {
      maxkey = -1;
      lua_pop(L, 1);
      break;
    }
  }

  lua_pop(L, 1);

  return maxkey;
}

static void enc_pop_stack(lua_State *L, ENC_DATA *data) {
  if (data->level < 0) {
    luaL_error(L, "encoder stack underflow");
  }
  ENC_DATA_STATE *state = &data->stack[data->level];

  lua_unref(L, state->lua_object_ref);
  state->lua_object_ref = LUA_NOREF;
  lua_unref(L, state->lua_key_ref);
  state->lua_key_ref = LUA_REFNIL;
  data->level--;
}

static void enc_push_stack(lua_State *L, ENC_DATA *data, int argno) {
  if (++data->level >= data->nlevels) {
    luaL_error(L, "encoder stack overflow");
  }
  lua_pushvalue(L, argno);
  ENC_DATA_STATE *state = &data->stack[data->level];
  state->lua_object_ref = lua_ref(L, 1);
  state->size = sjson_encoder_get_table_size(L, argno);
  state->offset = 0;         // We haven't started on this one yet
}

static int sjson_encoder(lua_State *L) {
  int nlevels = DEFAULT_DEPTH;
  int argno = 1;

  // Validate first arg is a table
  luaL_checktype(L, argno++, LUA_TTABLE);

  if (lua_type(L, argno) == LUA_TTABLE) {
    lua_getfield(L, argno, "depth");
    nlevels = lua_tointeger(L, argno);
    if (nlevels == 0) {
      nlevels = DEFAULT_DEPTH;
    }
    if (nlevels < 4) {
      nlevels = 4;
    }
    if (nlevels > 1000) {
      nlevels = 1000;
    }
    lua_pop(L, 1);
  }

  ENC_DATA *data = (ENC_DATA *) lua_newuserdata(L, sizeof(ENC_DATA) + nlevels * sizeof(ENC_DATA_STATE));

  // Associate its metatable
  luaL_getmetatable(L, "sjson.encoder");
  lua_setmetatable(L, -2);

  data->nlevels = nlevels;
  data->level = -1;
  data->stack = (ENC_DATA_STATE *) (data + 1);
  data->current_str_ref = LUA_NOREF;
  int i;
  for (i = 0; i < nlevels; i++) {
    data->stack[i].lua_object_ref = LUA_NOREF;
    data->stack[i].lua_key_ref = LUA_REFNIL;
  }
  enc_push_stack(L, data, 1);

  data->null_ref = LUA_REFNIL;

  if (lua_type(L, argno) == LUA_TTABLE) {
    luaL_unref(L, LUA_REGISTRYINDEX, data->null_ref);
    data->null_ref = LUA_NOREF;
    lua_getfield(L, argno, "null");
    data->null_ref = lua_ref(L, 1);
  }

  return 1;
}

static void encode_lua_object(lua_State *L, ENC_DATA *data, int argno, const char *prefix, const char *suffix) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);

  luaL_addstring(&b, prefix);

  int type = lua_type(L, argno);

  if (type == LUA_TSTRING) {
    // Check to see if it is the NULL value
    if (data->null_ref != LUA_REFNIL) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, data->null_ref);
      if (lua_equal(L, -1, -2)) {
        type = LUA_TNIL;
      } 
      lua_pop(L, 1);
    }
  }

  switch (type) {
    default:
      luaL_error(L, "Cannot encode type %d", type);
      break;

    case LUA_TLIGHTUSERDATA:
    case LUA_TNIL:
      luaL_addstring(&b, "null");
      break;

    case LUA_TBOOLEAN:
      luaL_addstring(&b, lua_toboolean(L, argno) ? "true" : "false");
      break;

    case LUA_TNUMBER:
    {
      lua_pushvalue(L, argno);
      size_t len;
      const char *str = lua_tolstring(L, -1, &len);
      char value[len + 1];
      strcpy(value, str);
      lua_pop(L, 1);
      luaL_addstring(&b, value);
      break;
    }

    case LUA_TSTRING:
    {
      luaL_addchar(&b, '"');
      size_t len;
      const char *str = lua_tolstring(L, argno, &len);
      while (len > 0) {
        if ((*str & 0xff) < 0x20) {
          char value[8];
          value[0] = '\\';

          char *d = value + 1;

          switch(*str) {
            case '\f':
              *d++ = 'f';
              break;
            case '\n':
              *d++ = 'n';
              break;
            case '\t':
              *d++ = 't';
              break;
            case '\r':
              *d++ = 'r';
              break;
            case '\b':
              *d++ = 'b';
              break;

            default:
              *d++ = 'u';
              *d++ = '0';
              *d++ = '0';
              *d++ = "0123456789abcdef"[(*str >> 4) & 0xf];
              *d++ = "0123456789abcdef"[(*str     ) & 0xf];
              break;

          }
          *d = '\0';
          luaL_addstring(&b, value);
        } else if (*str == '"') {
          luaL_addstring(&b, "\\\"");
        } else {
          luaL_addchar(&b, *str);
        }
        str++;
        len--;
      }
      luaL_addchar(&b, '"');
      break;
    }
  }

  luaL_addstring(&b, suffix);
  luaL_pushresult(&b);
}

static int sjson_encoder_next_value_is_table(lua_State *L) {
  int count = 10;

  while ((lua_type(L, -1) == LUA_TFUNCTION 
#ifdef LUA_TLIGHTFUNCTION
    || lua_type(L, -1) == LUA_TLIGHTFUNCTION
#endif
    ) && count-- > 0) {
    // call it and use the return value
    lua_call(L, 0, 1);          // Expecting replacement value
  }

  return (lua_type(L, -1) == LUA_TTABLE);
}

static void sjson_encoder_make_next_chunk(lua_State *L, ENC_DATA *data) {
  if (data->level < 0) {
    return;
  }

  luaL_Buffer b;
  luaL_buffinit(L, &b);

  // Ending condition
  while (data->level >= 0 && !b.lvl) {
    ENC_DATA_STATE *state = &data->stack[data->level];

    int finished = 0;

    if (state->size >= 0) {
      if (state->offset == 0) {
        // start of object or whatever
        luaL_addchar(&b, '[');
      } 
      if (state->offset == state->size << 1) {
        luaL_addchar(&b, ']');
        finished = 1;
      } else if ((state->offset & 1) == 0) {
        if (state->offset > 0) {
          luaL_addchar(&b, ',');
        }
      } else {
        // output the value
        lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_object_ref);
        lua_rawgeti(L, -1, (state->offset >> 1) + 1);
        if (sjson_encoder_next_value_is_table(L)) {
          enc_push_stack(L, data, -1);
          lua_pop(L, 2);
          state->offset++;
          continue;
        }
        encode_lua_object(L, data, -1, "", "");
        lua_remove(L, -2);
        lua_remove(L, -2);
        luaL_addvalue(&b);
      }

      state->offset++;
    } else {
      lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_object_ref);
      // stack now contains: -1 => table
      lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_key_ref);
      // stack now contains: -1 => nil or key; -2 => table

      if (lua_next(L, -2)) {
        // save the key
        if (state->offset & 1) {
          lua_unref(L, state->lua_key_ref);
          state->lua_key_ref = LUA_NOREF;
          // Duplicate the key
          lua_pushvalue(L, -2);
          state->lua_key_ref = lua_ref(L, 1);
        }

        if ((state->offset & 1) == 0) {
          // copy the key so that lua_tostring does not modify the original
          lua_pushvalue(L, -2);
          // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
          // key
          lua_tostring(L, -1);
          encode_lua_object(L, data, -1, state->offset ? "," : "{", ":");
          lua_remove(L, -2);
          lua_remove(L, -2);
          lua_remove(L, -2);
          lua_remove(L, -2);
        } else {
          if (sjson_encoder_next_value_is_table(L)) {
            enc_push_stack(L, data, -1);
            lua_pop(L, 3);
            state->offset++;
            continue;
          }
          encode_lua_object(L, data, -1, "", "");
          lua_remove(L, -2);
          lua_remove(L, -2);
          lua_remove(L, -2);
        }
        luaL_addvalue(&b);
      } else {
        lua_pop(L, 1);
        // We have got to the end
        luaL_addchar(&b, '}');
        finished = 1;
      }

      state->offset++;
    }

    if (finished) {
      enc_pop_stack(L, data);
    }
  }
  luaL_pushresult(&b);
  data->current_str_ref = lua_ref(L, 1);
  data->offset = 0;
}

static int sjson_encoder_read_int(lua_State *L, ENC_DATA *data, int readsize) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);

  size_t len;

  do {
    // Fill the buffer with (up to) readsize characters
    if (data->current_str_ref != LUA_NOREF) {
      // this is not allowed
      lua_rawgeti(L, LUA_REGISTRYINDEX, data->current_str_ref);
      const char *str = lua_tolstring(L, -1, &len);

      lua_pop(L, 1); // Note that we still have the string referenced so it can't go away

      int amnt = len - data->offset;;
      if (amnt > readsize) {
        amnt = readsize;
      }
      luaL_addlstring(&b, str + data->offset, amnt);
      data->offset += amnt;
      readsize -= amnt;

      if (data->offset == len) {
        lua_unref(L, data->current_str_ref);
        data->current_str_ref = LUA_NOREF;
      }
    }

    if (readsize > 0) {
      // Make the next chunk
      sjson_encoder_make_next_chunk(L, data);
    }

  } while (readsize > 0 && data->current_str_ref != LUA_NOREF);

  luaL_pushresult(&b);

  lua_tolstring(L, -1, &len);

  if (len == 0) {
    // we have got to the end
    lua_pop(L, 1);
    return 0;
  }

  return 1;
}

static int sjson_encoder_read(lua_State *L) {
  ENC_DATA *data = (ENC_DATA *)luaL_checkudata(L, 1, "sjson.encoder");

  int readsize = 1024;
  if (lua_type(L, 2) == LUA_TNUMBER) {
    readsize = lua_tointeger(L, 2);
    if (readsize < 1) {
      readsize = 1;
    }
  }

  return sjson_encoder_read_int(L, data, readsize);
}

static int sjson_encode(lua_State *L) {
  sjson_encoder(L);

  ENC_DATA *data = (ENC_DATA *)luaL_checkudata(L, -1, "sjson.encoder");

  int rc = sjson_encoder_read_int(L, data, 1000000);

  lua_remove(L, -(rc + 1));

  return rc;
}

static int sjson_encoder_destructor(lua_State *L) {
  ENC_DATA *data = (ENC_DATA *)luaL_checkudata(L, 1, "sjson.encoder");

  int i;

  for (i = 0; i < data->nlevels; i++) {
    luaL_unref(L, LUA_REGISTRYINDEX, data->stack[i].lua_object_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, data->stack[i].lua_key_ref);
  }

  luaL_unref(L, LUA_REGISTRYINDEX, data->null_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, data->current_str_ref);

  DBG_PRINTF("Destructor called\n");

  return 0;
}

#ifdef LOCAL_LUA
static const luaL_Reg sjson_encoder_map[] = {
  { "read", sjson_encoder_read },
  { "__gc", sjson_encoder_destructor },
  { NULL, NULL }
};

static const luaL_Reg sjson_decoder_map[] = {
  { "write", sjson_decoder_write },
  { "result", sjson_decoder_result },
  { "__gc", sjson_decoder_destructor },
  { NULL, NULL }
};

static const luaL_Reg sjsonlib[] = {
  { "decode", sjson_decode },
  { "decoder", sjson_decoder },
  { "encode", sjson_encode },
  { "encoder", sjson_encoder },
  {NULL, NULL}
};
#else
static const LUA_REG_TYPE sjson_encoder_map[] = {
  { LSTRKEY( "read" ),                    LFUNCVAL( sjson_encoder_read ) },
  { LSTRKEY( "__gc" ),                    LFUNCVAL( sjson_encoder_destructor ) },
  { LSTRKEY( "__index" ),                 LROVAL( sjson_encoder_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE sjson_decoder_map[] = {
  { LSTRKEY( "write" ),                   LFUNCVAL( sjson_decoder_write ) },
  { LSTRKEY( "result" ),                  LFUNCVAL( sjson_decoder_result ) },
  { LSTRKEY( "__gc" ),                    LFUNCVAL( sjson_decoder_destructor ) },
  { LSTRKEY( "__index" ),                 LROVAL( sjson_decoder_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE sjson_map[] = {
  { LSTRKEY( "encode" ),                  LFUNCVAL( sjson_encode ) },
  { LSTRKEY( "decode" ),                  LFUNCVAL( sjson_decode ) },
  { LSTRKEY( "encoder" ),                 LFUNCVAL( sjson_encoder ) },
  { LSTRKEY( "decoder" ),                 LFUNCVAL( sjson_decoder ) },
  { LSTRKEY( "NULL" ),                    LUDATA( 0 ) },
  { LNILKEY, LNILVAL }
};
#endif

LUALIB_API int luaopen_sjson (lua_State *L) {
#ifdef LOCAL_LUA
  luaL_register(L, LUA_SJSONLIBNAME, sjsonlib);
  lua_getglobal(L, LUA_SJSONLIBNAME);
  lua_pushstring(L, "NULL");
  lua_pushlightuserdata(L, 0);
  lua_settable(L, -3);
  lua_pop(L, 1);
  luaL_newmetatable(L, "sjson.encoder");
  luaL_register(L, NULL, sjson_encoder_map);
  lua_setfield(L, -1, "__index");
  luaL_newmetatable(L, "sjson.decoder");
  luaL_register(L, NULL, sjson_decoder_map);
  lua_setfield(L, -1, "__index");
#else
  luaL_rometatable(L, "sjson.decoder", (void *)sjson_decoder_map);  
  luaL_rometatable(L, "sjson.encoder", (void *)sjson_encoder_map);  
#endif
  return 1;
}

#ifndef LOCAL_LUA
NODEMCU_MODULE(SJSON, "sjson", sjson_map, luaopen_sjson);
#endif
