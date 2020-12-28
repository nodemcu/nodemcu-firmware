/*
** The pipe algo is somewhat similar to the luaL_Buffer algo, except that it
** uses table to store the LUAL_BUFFERSIZE byte array chunks instead of the
** stack.  Writing is always to the last UD in the table and overflow pushes a
** new UD to the end of the table.  Reading is always from the first UD in the
** table and underrun removes the first UD to shift a new one into slot 2. (Slot
** 1 of the table is reserved for the pipe reader function with 0 denoting no
** reader.)
**
** Reads and writes may span multiple UD buffers and if the read spans multiple
** UDs then the parts are collected as strings on the Lua stack and then
** concatenated with a lua_concat().
**
** Note that pipe tables also support the undocumented length and tostring
** operators for debugging puposes, so if p is a pipe then #p[i] gives the
** effective length of pipe slot i and printing p[i] gives its contents.
**
** The pipe library also supports the automatic scheduling of a reader task.
** This is declared by including a Lua CB function and an optional prioirty for
** it to execute at in the pipe.create() call. The reader task may or may not
** empty the FIFO (and there is also nothing to stop the task also writing to
** the FIFO.  The reader automatically reschedules itself if the pipe contains
** unread content.
**
** The reader tasks may be interleaved with other tasks that write to the pipe
** and others that don't. Any task writing to the pipe will also trigger the
** posting of a read task if one is not already pending.  In this way at most
** only one pending reader task is pending, and this prevents overrun of the
** task queueing system.
**
** Implementation Notes:
**
** -  The Pipe slot 1 is used to store the Lua CB function reference of the
**    reader task. Note that is actually an auxiliary wrapper around the
**    supplied Lua CB function, and this wrapper also uses upvals to store
**    internal pipe state.  The remaining slots are the Userdata buffer chunks.
**
** -  This internal state needs to be shared with the pipe_write function, but a
**    limitation of Lua 5.1 is that C functions cannot share upvals; to avoid
**    this constraint, this function is also denormalised to act as the
**    pipe_write function: if Arg1 is the pipe then its a pipe:write() otherwise
**    its a CB wrapper.
**
** Also note that the pipe module is used by the Lua VM and therefore the create
** read, and unread methods are exposed as directly callable C functions. (Write
** is available through pipe[1].)
**
** Read the docs/modules/pipe.md documentation for a functional description.
*/

#include "module.h"
#include "lauxlib.h"
#include <string.h>
#include "platform.h"

#define INVALID_LEN ((unsigned)-1)

#define LUA_USE_MODULES_PIPE

typedef struct buffer {
  int start, end;
  char buf[LUAL_BUFFERSIZE];
} buffer_t;


#define AT_TAIL       0x00
#define AT_HEAD       0x01
#define WRITING       0x02

static buffer_t *checkPipeUD (lua_State *L, int ndx);
static buffer_t *newPipeUD(lua_State *L, int ndx, int n);
static int pipe_write_aux(lua_State *L);
LROT_TABLE(pipe_meta);

/* Validation and utility functions */
                                                                  // [-0, +0, v]
static buffer_t *checkPipeTable (lua_State *L, int tbl, int flags) {
  int m = lua_gettop(L), n = lua_objlen(L, tbl);
  if (lua_istable(L, tbl) && lua_getmetatable(L, tbl)) {
    lua_pushrotable(L, LROT_TABLEREF(pipe_meta));/* push comparison metatable */
    if (lua_rawequal(L, -1, -2)) {                       /* check these match */
      buffer_t *ud;
      if (n == 1) {
        ud = (flags & WRITING) ? newPipeUD(L, tbl, 2) : NULL;
      } else {
        int i = flags & AT_HEAD ? 2 : n;        /* point to head or tail of T */
        lua_rawgeti(L, tbl, i);                               /* and fetch UD */
        ud = checkPipeUD(L, -1);
      }
      lua_settop(L, m);
      return ud;                            /* and return ptr to buffer_t rec */
    }
  }
  luaL_argerror(L, tbl, "pipe table");
  return NULL;                               /* NORETURN avoid compiler error */
}

static buffer_t *checkPipeUD (lua_State *L, int ndx) {            // [-0, +0, v]
  luaL_checktype(L, ndx, LUA_TUSERDATA);                 /* NORETURN on error */
  buffer_t *ud = lua_touserdata(L, ndx);
  if (ud && lua_getmetatable(L, ndx)) {          /* Get UD and its metatable? */
    lua_pushrotable(L, LROT_TABLEREF(pipe_meta));   /* push correct metatable */
    if (lua_rawequal(L, -1, -2)) {                         /* Do these match? */
      lua_pop(L, 2);                                /* remove both metatables */
      return ud;                            /* and return ptr to buffer_t rec */
    }
  }
}

/* Create new buffer chunk at `n` in the table which is at stack `ndx` */
static buffer_t *newPipeUD(lua_State *L, int ndx, int n) {   // [-0,+0,-]
  buffer_t *ud = (buffer_t *) lua_newuserdata(L, sizeof(buffer_t));
  lua_pushrotable(L, LROT_TABLEREF(pipe_meta));         /* push the metatable */
	lua_setmetatable(L, -2);                /* set UD's metabtable to pipe_meta */
	ud->start = ud->end = 0;
  lua_rawseti(L, ndx, n);                                    /* T[n] = new UD */
  return ud;                                         /* ud points to new T[n] */
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

/*
** Read CB Initiator AND pipe_write. If arg1 == the pipe, then this is a pipe
** write(); otherwise it is the Lua CB wapper for the task post. This botch
** allows these two functions to share Upvals within the Lua 5.1 VM:
*/
#define UVpipe  lua_upvalueindex(1)  // The pipe table object
#define UVfunc  lua_upvalueindex(2)  // The CB's Lua function
#define UVprio  lua_upvalueindex(3)  // The task priority
#define UVstate lua_upvalueindex(4)  // Pipe state;
#define CB_NOT_USED       0
#define CB_ACTIVE         1
#define CB_WRITE_UPDATED  2
#define CB_QUIESCENT      4
/*
** Note that nothing precludes the Lua CB function from itself writing to the
** pipe and in this case this routine will call itself recursively.
**
** The Lua CB itself takes the pipe object as a parameter and returns an
** optional boolean to force or to suppress automatic retasking if needed.  If
** omitted, then the default is to repost if the pipe is not empty, otherwise
** the task chain is left to lapse.
*/
static int pipe_write_and_read_poster (lua_State *L) {
  int state = lua_tointeger(L, UVstate);
  if (lua_rawequal(L, 1, UVpipe)) {
    /* arg1 == the pipe, so this was invoked as a pipe_write() */
    if (pipe_write_aux(L) && state && !(state & CB_WRITE_UPDATED)) {
     /*
      * if this resulted in a write and not already in a CB and not already
      * toggled the write update then post the task
      */
      state |= CB_WRITE_UPDATED;
      lua_pushinteger(L, state);
      lua_replace(L, UVstate);             /* Set CB state write updated flag */
      if (state == CB_QUIESCENT | CB_WRITE_UPDATED) {
        lua_rawgeti(L, 1, 1);                      /* Get CB ref from pipe[1] */
        luaL_posttask(L, (int) lua_tointeger(L, UVprio));  /* and repost task */
      }
    }

  } else if (state != CB_NOT_USED) {
    /* invoked by the luaN_taskpost() so call the Lua CB */
    int repost;                  /* can take the values CB_WRITE_UPDATED or 0 */
    lua_pushinteger(L, CB_ACTIVE);             /* CB state set to active only */
    lua_replace(L, UVstate);
    lua_pushvalue(L, UVfunc);                              /* Lua CB function */
    lua_pushvalue(L, UVpipe);                                   /* pipe table */
    lua_call(L, 1, 1);                    /* Errors are thrown back to caller */
   /*
    * On return from the Lua CB, the task is never reposted if the pipe is empty.
    * If it is not empty then the Lua CB return status determines when reposting
    * occurs:
    *  -  true  = repost
    *  -  false = don't repost
    *  -  nil  = only repost if there has been a write update.
    */
    if (lua_isboolean(L,-1)) {
      repost = (lua_toboolean(L, -1) == true &&
                lua_objlen(L, UVpipe) > 1) ? CB_WRITE_UPDATED : 0;
    } else {
      repost = state & CB_WRITE_UPDATED;
    }
    state = CB_QUIESCENT | repost;
    lua_pushinteger(L, state);                         /* Update the CB state */
    lua_replace(L, UVstate);

    if (repost) {
      lua_rawgeti(L, UVpipe, 1);                   /* Get CB ref from pipe[1] */
      luaL_posttask(L, (int) lua_tointeger(L, UVprio));    /* and repost task */
    }
  }
  return 0;
}

/* Lua callable methods. Since the metatable is linked to both the pipe table */
/* and the userdata entries the __len & __tostring functions must handle both */

// Lua: buf = pipe.create()
int pipe_create(lua_State *L) {
  int prio = -1;
  lua_settop(L, 2);                                     /* fix stack sze as 2 */

  if (!lua_isnil(L, 1)) {
    luaL_checktype(L, 1, LUA_TFUNCTION);   /* non-nil arg1 must be a function */
    if (lua_isnil(L, 2)) {
      prio = PLATFORM_TASK_PRIORITY_MEDIUM;
    } else {
      prio = (int) lua_tointeger(L, 2);
      luaL_argcheck(L, prio >= PLATFORM_TASK_PRIORITY_LOW &&
                       prio <= PLATFORM_TASK_PRIORITY_HIGH, 2,
                       "invalid priority");
    }
  }

  lua_createtable (L, 1, 0);                             /* create pipe table */
	lua_pushrotable(L, LROT_TABLEREF(pipe_meta));
	lua_setmetatable(L, -2);        /* set pipe table's metabtable to pipe_meta */

  lua_pushvalue(L, -1);                                   /* UV1: pipe object */
  lua_pushvalue(L, 1);                                    /* UV2: CB function */
  lua_pushinteger(L, prio);                             /* UV3: task priority */
  lua_pushinteger(L, prio == -1 ? CB_NOT_USED : CB_QUIESCENT);
  lua_pushcclosure(L, pipe_write_and_read_poster, 4);  /* aux func for C task */
	lua_rawseti(L, -2, 1);                                 /* and write to T[1] */
	return 1;                                               /* return the table */
}

// len = #pipeobj[i]
static int pipe__len (lua_State *L) {
   if (lua_istable(L, 1)) {
    lua_pushinteger(L, lua_objlen(L, 1));
  } else {
    buffer_t *ud = checkPipeUD(L, 1);
    lua_pushinteger(L, ud->end - ud->start);
  }
  return 1;
}

//Lua s = pipeUD:tostring()
static int pipe__tostring (lua_State *L) {
  if (lua_istable(L, 1)) {
    lua_pushfstring(L, "Pipe: %p", lua_topointer(L, 1));
  } else {
    buffer_t *ud = checkPipeUD(L, 1);
    lua_pushlstring(L, ud->buf + ud->start, ud->end - ud->start);
  }
  return 1;
}

// Lua: rec = p:read(end_or_delim)                           // also [-2, +1,- ]
int pipe_read(lua_State *L) {
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
        n = 0;                         /* force loop exit because delim found */
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
      for (i = 2; i < nUD; i++) {                         /* for i = 2, nUD-1 */
        lua_rawgeti(L, 1, i+1); lua_rawseti(L, 1, i);        /* T[i] = T[i+1] */
      }
      lua_pushnil(L); lua_rawseti(L, 1, nUD--);                 /* T[n] = nil */
      if (nUD>1) {
        lua_rawgeti(L, 1, 2);
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
int pipe_unread(lua_State *L) {
  size_t l = INVALID_LEN;
  const char *s = lua_tolstring(L, 2, &l);
  if (l==0)
    return 0;
  luaL_argcheck(L, l != INVALID_LEN, 2, "must be a string");
  buffer_t *ud = checkPipeTable(L, 1, AT_HEAD | WRITING);

  do {
    int used = ud->end - ud->start;
    int lrem = LUAL_BUFFERSIZE-used;

    if (used == LUAL_BUFFERSIZE) {
      /* If the current UD is full insert a new UD at T[2] */
      int i, nUD = lua_objlen(L, 1);
      for (i = nUD; i > 1; i--) {                         /* for i = nUD,1,-1 */
        lua_rawgeti(L, 1, i); lua_rawseti(L, 1, i+1);        /* T[i+1] = T[i] */
      }
      ud = newPipeUD(L, 1, 2);
      used = 0; lrem = LUAL_BUFFERSIZE;

      /* Filling leftwards; make this chunk "empty but at the right end" */
      ud->start = ud->end = LUAL_BUFFERSIZE;
    } else if (ud->start < l) {
      /* If the unread can't fit it before the start, shift content to end */
      memmove(ud->buf + lrem,
              ud->buf + ud->start, used); /* must be memmove not cpy */
      ud->start = lrem; ud->end = LUAL_BUFFERSIZE;
    }

    if (l <= (unsigned )lrem)
      break;

    /* If we're here then the remaining string is strictly longer than the */
    /* remaining buffer space; top off the buffer before looping around again */
    l -= lrem;
    memcpy(ud->buf, s + l, lrem);
    ud->start = 0;

  } while(1);

  /* Copy any residual tail to the UD buffer.
   * Here, ud != NULL and 0 <= l <= ud->start */

  ud->start -= l;
  memcpy(ud->buf + ud->start, s, l);
	return 0;
}

// Lua: buf:write(some_string)
static int pipe_write_aux(lua_State *L) {
  size_t l = INVALID_LEN;
  const char *s = lua_tolstring(L, 2, &l);
//dbg_printf("pipe write(%u): %s", l, s);
  if (l==0)
    return false;
  luaL_argcheck(L, l != INVALID_LEN, 2, "must be a string");
  buffer_t *ud = checkPipeTable(L, 1, AT_TAIL | WRITING);

  do {
    int used = ud->end - ud->start;

    if (used == LUAL_BUFFERSIZE) {
     /* If the current UD is full insert a new UD at T[end] */
      ud = newPipeUD(L, 1, lua_objlen(L, 1)+1);
      used = 0;
    } else if (LUAL_BUFFERSIZE - ud->end < l) {
      /* If the write can't fit it at the end then shift content to the start */
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
	return true;
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

// return number of records
static int pipe_nrec (lua_State *L) {
  lua_pushinteger(L, lua_objlen(L, 1) - 1);
  return 1;
}

LROT_BEGIN(pipe_funcs, NULL, 0)
  LROT_FUNCENTRY( __len, pipe__len )
  LROT_FUNCENTRY( __tostring, pipe__tostring )
  LROT_FUNCENTRY( read, pipe_read )
  LROT_FUNCENTRY( reader, pipe_reader )
  LROT_FUNCENTRY( unread, pipe_unread )
  LROT_FUNCENTRY( nrec, pipe_nrec )
LROT_END(pipe_funcs, NULL, 0)

/* Using a index func is needed because the write method is at pipe[1] */
static int pipe__index(lua_State *L) {
  lua_settop(L,2);
  const char *k=lua_tostring(L,2);
  if(!strcmp(k,"write")){
    lua_rawgeti(L, 1, 1);
  } else {
    lua_pushrotable(L, LROT_TABLEREF(pipe_funcs));
    lua_replace(L, 1);
    lua_rawget(L, 1);
  }
  return 1;
}

LROT_BEGIN(pipe_meta, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __index, pipe__index)
  LROT_FUNCENTRY( __len, pipe__len )
  LROT_FUNCENTRY( __tostring, pipe__tostring )
LROT_END(pipe_meta, NULL, LROT_MASK_INDEX)

LROT_BEGIN(pipe, NULL, 0)
  LROT_FUNCENTRY( create, pipe_create )
LROT_END(pipe, NULL, 0)

NODEMCU_MODULE(PIPE, "pipe", pipe, NULL);
