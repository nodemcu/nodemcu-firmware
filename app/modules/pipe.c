/*
** The pipe algo is somewhat similar to the luaL_Buffer algo, except that it uses
** table to store the LUAL_BUFFERSIZE byte array chunks instead of the stack.
** Writing is always to the last UD in the table and overflow pushes a new UD to
** the end of the table.  Reading is always from the first UD in the table and
** underrun removes the first UD to shift a new one into slot 1.
**
** Reads and writes may span multiple UD buffers and if the read spans multiple UDs
** then the parts are collected as strings on the Lua stack and then concatenated
** with a `lua_concat()`.
**
** Note that pipes also support the undocumented length and tostring operators
** for debugging puposes, so if p is a pipe then #p[1] gives the effective
** length of pipe slot 1 and printing p[1] gives its contents
**
** Read the docs/modules/pipe.md documentation for a functional description.
*/

#include "module.h"
#include "lauxlib.h"
#include <string.h>

#define INVALID_LEN ((unsigned)-1)

#define LUA_USE_MODULES_PIPE

typedef struct buffer {
  int start, end;
  char buf[LUAL_BUFFERSIZE];
} buffer_t;

LROT_TABLE(pipe_meta)

/* Validation and utility functions */

#define AT_HEAD 1
#define AT_TAIL 0

static buffer_t *checkPipeUD (lua_State *L, int ndx) {            // [-0, +0, v]
  buffer_t *ud = lua_touserdata(L, ndx);
  if (ud && lua_getmetatable(L, ndx)) {          /* Get UD and its metatable? */
    lua_pushrotable(L, LROT_TABLEREF(pipe_meta));   /* push correct metatable */
    if (lua_rawequal(L, -1, -2)) {                         /* Do these match? */
      lua_pop(L, 2);                                /* remove both metatables */
      return ud;                            /* and return ptr to buffer_t rec */
    }
  }
  if (!lua_istable(L,ndx))
    luaL_typerror(L, ndx, "pipeUD");                        /* NORETURN error */
  return NULL;                                         /* keep compiler happy */
}

static buffer_t *newPipeUD(lua_State *L, int ndx, int n) {   // [-0,+0,-]
  buffer_t *ud = (buffer_t *) lua_newuserdata(L, sizeof(buffer_t));
  lua_pushrotable(L, LROT_TABLEREF(pipe_meta));         /* push the metatable */
	lua_setmetatable(L, -2);                /* set UD's metabtable to pipe_meta */
	ud->start = ud->end = 0;
  lua_rawseti(L, ndx, n);                                 /* T[#T+1] = new UD */
  return ud;                                        /* ud points to new T[#T] */
}

static buffer_t *checkPipeTable (lua_State *L, int tbl, int head) {//[-0, +0, v]
  int m = lua_gettop(L), n = lua_objlen(L, tbl);
  if (lua_type(L, tbl) == LUA_TTABLE && lua_getmetatable(L, tbl)) {
    lua_pushrotable(L, LROT_TABLEREF(pipe_meta));/* push comparison metatable */
    if (lua_rawequal(L, -1, -2)) {                       /* check these match */
      buffer_t *ud;
      if (n == 0) {
        ud = head ? NULL : newPipeUD(L, tbl, 1);
      } else {
        int i = head ? 1 : n;                   /* point to head or tail of T */
        lua_rawgeti(L, tbl, i);                               /* and fetch UD */
        ud = checkPipeUD(L, -1);
      }
      lua_settop(L, m);
      return ud;                            /* and return ptr to buffer_t rec */
    }
  }
  luaL_typerror(L, tbl, "pipe table");
  return NULL;                               /* NORETURN avoid compiler error */
}

#define CHAR_DELIM      -1
#define CHAR_DELIM_KEEP -2
static char getsize_delim (lua_State *L, int ndx, int *len) {     // [-0, +0, v]
  char delim;
  int  n;
  if( lua_type( L, ndx ) == LUA_TNUMBER ) {
    n = luaL_checkinteger( L, ndx );
    luaL_argcheck(L, n > 0, ndx, "invalid chunk size");
    delim = '\0';
  } else if (lua_isnil(L, ndx)) {
    n = CHAR_DELIM;
    delim = '\n';
  } else {
    size_t ldelim;
    const char *s = lua_tolstring( L, 2, &ldelim);
    n = ldelim == 2 && s[1] == '+' ? CHAR_DELIM_KEEP : CHAR_DELIM ;
    luaL_argcheck(L, ldelim + n == 0, ndx, "invalid delimiter");
    delim = s[0];
  }
  if (len)
    *len = n;
  return delim;
}

/* Lua callable methods */

//Lua s = pipeUD:tostring()
static int pipe__tostring (lua_State *L) {
  if (lua_type(L, 1) == LUA_TTABLE) {
    lua_pushfstring(L, "Pipe: %p", lua_topointer(L, 1));
  } else {
    buffer_t *ud = checkPipeUD(L, 1);
    lua_pushlstring(L, ud->buf + ud->start, ud->end - ud->start);
  }
  return 1;
}

// len = #pipeobj[1]
static int pipe__len (lua_State *L) {
  if (lua_type(L, 1) == LUA_TTABLE) {
    lua_pushinteger(L, lua_objlen(L, 1));
  } else {
    buffer_t *ud = checkPipeUD(L, 1);
    lua_pushinteger(L, ud->end - ud->start);
  }
  return 1;
}

// Lua: buf = pipe.create()
static int pipe_create(lua_State *L) {
  lua_createtable (L, 1, 0);
	lua_pushrotable(L, LROT_TABLEREF(pipe_meta));
	lua_setmetatable(L, 1);              /* set table's metabtable to pipe_meta */
	return 1;                                               /* return the table */
}

// Lua: rec = p:read(end_or_delim)                           // also [-2, +1,- ]
static int pipe_read(lua_State *L) {
  buffer_t *ud = checkPipeTable(L, 1, AT_HEAD);
  int i, k=0, n;
  lua_settop(L,2);
  const char delim = getsize_delim(L, 2, &n);
  lua_pop(L,1);

  while (ud && n) {
    int want, used, avail = ud->end - ud->start;

    if (n < 0 /* one of the CHAR_DELIM flags */) {
      /* getting a delimited chunk so scan for delimiter */
      for (i = ud->start; i < ud->end && ud->buf[i] != delim; i++) {}
      /* Can't have i = ud->end and ud->buf[i] == delim so either */
      /* we've scanned full buffer avail OR we've hit a delim char */
      if (i == ud->end) {
        want = used = avail;        /* case where we've scanned without a hit */
      } else {
        want = used = i + 1 - ud->start;      /* case where we've hit a delim */
        if (n == CHAR_DELIM)
          want--;
      }
    } else {
      want = used = (n < avail) ? n : avail;
      n -= used;
    }
    lua_pushlstring(L, ud->buf + ud->start, want);            /* part we want */
    k++;
    ud->start += used;
    if (ud->start == ud->end) {
      /* shift the pipe array down overwriting T[1] */
      int nUD = lua_objlen(L, 1);
      for (i = 1; i < nUD; i++) {                         /* for i = 1, nUD-1 */
        lua_rawgeti(L, 1, i+1); lua_rawseti(L, 1, i);        /* T[i] = T[i+1] */  
      }
      lua_pushnil(L); lua_rawseti(L, 1, nUD--);                 /* T[n] = nil */
      if (nUD) {
        lua_rawgeti(L, 1, 1);
        ud = checkPipeUD(L, -1);
        lua_pop(L, 1);
      } else {
        ud = NULL;
      }
    }
  }
  if (k)
    lua_concat(L, k);
  else
    lua_pushnil(L);
  return 1;
}

// Lua: buf:unread(some_string)
static int pipe_unread(lua_State *L) {
  size_t l = INVALID_LEN;
  const char *s = lua_tolstring(L, 2, &l);
  if (l==0)
    return 0;
  luaL_argcheck(L, l != INVALID_LEN, 2, "must be a string");
  buffer_t *ud = checkPipeTable(L, 1, AT_HEAD);

  do {
    int used = ud->end - ud->start, lrem = LUAL_BUFFERSIZE-used;

    if (used == LUAL_BUFFERSIZE) {
      int i, nUD = lua_objlen(L, 1);
      for (i = nUD; i > 0; i--) {                       /* for i = nUD-1,1,-1 */
        lua_rawgeti(L, 1, i); lua_rawseti(L, 1, i+1);        /* T[i+1] = T[i] */  
      }
      ud = newPipeUD(L, 1, 1);
      used = 0; lrem = LUAL_BUFFERSIZE;
    } else if (ud->end < LUAL_BUFFERSIZE) {
      memmove(ud->buf + lrem, 
              ud->buf + ud->start, used); /* must be memmove not cpy */
    }
    ud->start = lrem; ud->end = LUAL_BUFFERSIZE;

    if (l <= (unsigned )lrem)
      break;

    /* If we've got here then the remaining string is strictly longer than the */
    /* remaining buffer space, so top off the buffer before looping around again */
    l -= lrem;    
    memcpy(ud->buf, s + l, lrem);
    ud->start = 0;

  } while(1);

  /* Copy any residual tail to the UD buffer.  Note that this is l>0 and  */
  ud->start -= l;  
  memcpy(ud->buf + ud->start, s, l);
	return 0;
}

// Lua: buf:write(some_string)
static int pipe_write(lua_State *L) {
  size_t l = INVALID_LEN;
  const char *s = lua_tolstring(L, 2, &l);
  if (l==0)
    return 0;
  luaL_argcheck(L, l != INVALID_LEN, 2, "must be a string");
  buffer_t *ud = checkPipeTable(L, 1, AT_TAIL);

  do {
    int used = ud->end - ud->start;

    if (used == LUAL_BUFFERSIZE) {
      ud = newPipeUD(L, 1, lua_objlen(L, 1)+1);
      used = 0;
    } else if (ud->start) {
      memmove(ud->buf, ud->buf + ud->start, used); /* must be memmove not cpy */
      ud->start = 0; ud->end = used;
    }

    int lrem = LUAL_BUFFERSIZE-used;
    if (l <= (unsigned )lrem)
      break;

    /* If we've got here then the remaining string is longer than the buffer */
    /* space left, so top off the buffer before looping around again */
    memcpy(ud->buf + ud->end, s, lrem);
    ud->end += lrem;
    l       -= lrem;
    s       += lrem;

  } while(1);

  /* Copy any residual tail to the UD buffer.  Note that this is l>0 and  */
  memcpy(ud->buf + ud->end, s, l);
  ud->end += l;
	return 0;
}

// Lua: fread = pobj:reader(1400) -- or other number
//      fread = pobj:reader('\n') -- or other delimiter (delim is stripped)
//      fread = pobj:reader('\n+') -- or other delimiter (delim is retained)
#define TBL lua_upvalueindex(1)
#define N   lua_upvalueindex(2)
static int pipe_read_aux(lua_State *L) {
  lua_settop(L, 0);                /* ignore args since we use upvals instead */
  lua_pushvalue(L, TBL);
  lua_pushvalue(L, N);
  return pipe_read(L);
}
static int pipe_reader(lua_State *L) {
  lua_settop(L,2);
  checkPipeTable (L, 1,AT_HEAD);
  getsize_delim(L, 2, NULL);
  lua_pushcclosure(L, pipe_read_aux, 2);      /* return (closure, nil, table) */
  return 1;
}


LROT_BEGIN(pipe_meta)
  LROT_TABENTRY(  __index, pipe_meta)
  LROT_FUNCENTRY( __len, pipe__len )
  LROT_FUNCENTRY( __tostring, pipe__tostring )
  LROT_FUNCENTRY( read, pipe_read )
  LROT_FUNCENTRY( reader, pipe_reader )
  LROT_FUNCENTRY( unread, pipe_unread )
  LROT_FUNCENTRY( write, pipe_write )
LROT_END( pipe_meta, NULL, LROT_MASK_INDEX )


LROT_BEGIN(pipe)
  LROT_FUNCENTRY( create, pipe_create )
LROT_END( lb, NULL, 0 )


NODEMCU_MODULE(PIPE, "pipe", pipe, NULL);
