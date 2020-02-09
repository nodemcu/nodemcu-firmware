/*
** NodeMCU Lua 5.1 main initiator and comand interpreter
** See Copyright Notice in lua.h
**
** Note this is largely a backport of some new Lua 5.3 version but 
** with API changes for Lua 5.1 compatability
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "user_version.h"
#include "driver/input.h"

#define lua_c

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "legc.h"
#include "os_type.h"

extern int debug_errorfb (lua_State *L);
extern int pipe_create(lua_State *L);
extern int pipe_read(lua_State *L);
extern int pipe_unread(lua_State *L);

#ifndef LUA_INIT_STRING
#define LUA_INIT_STRING "@init.lua"
#endif

static int MLref = LUA_NOREF;
lua_State *globalL = NULL;

static int pmain (lua_State *L);
static int dojob (lua_State *L);

static void l_message (const char *msg) {
  luai_writestringerror("%s\n", msg);
}

static int report (lua_State *L, int status) {
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(msg);
    lua_pop(L, 1);
  }
  return status;
}

static void l_print(lua_State *L, int n) {
  lua_getglobal(L, "print");
  lua_insert(L, -n-1);
  if (lua_pcall(L, n, 0, 0) != 0)
    l_message(lua_pushfstring(L, "error calling " LUA_QL("print") " (%s)",
                              lua_tostring(L, -1)));
}

static int traceback (lua_State *L) {
  if (lua_isstring(L, 1)) {
    lua_pushlightfunction(L, &debug_errorfb);
    lua_pushvalue(L, 1);                                /* pass error message */
    lua_pushinteger(L, 2);                /* skip this function and traceback */
    lua_call(L, 2, 1);                                /* call debug.traceback */
  }
  lua_settop(L, 1);
  return 1;
}

static int docall (lua_State *L, int narg, int clear) {
  int status;
  int base = lua_gettop(L) - narg;                          /* function index */
  lua_pushlightfunction(L, &traceback);            /* push traceback function */
  lua_insert(L, base);                         /* put it under chunk and args */
  status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
  lua_remove(L, base);                           /* remove traceback function */
  /* force a complete garbage collection in case of errors */
  if (status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
  return status;
}

static void print_version (lua_State *L) {
  lua_pushliteral (L, "\n" NODE_VERSION " build " BUILD_DATE 
                      " powered by " LUA_RELEASE " on SDK ");
  lua_pushstring (L, SDK_VERSION);
  lua_concat (L, 2);
  const char *msg = lua_tostring (L, -1);
  l_message (msg);
  lua_pop (L, 1);
}

static int dofile (lua_State *L, const char *name) {
  int status = luaL_loadfile(L, name) || docall(L, 0, 1);
  return report(L, status);
}

static int dostring (lua_State *L, const char *s, const char *name) {
  int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0, 1);
  return report(L, status);
}

static const char *get_prompt (lua_State *L, int firstline) {
  const char *p;
  lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2");
  p = lua_tostring(L, -1);
  if (p == NULL) p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
  lua_pop(L, 1);  /* remove global */
  return p;
}

#define EOFMARK  LUA_QL("<eof>")
static int incomplete (lua_State *L, int status) {
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    if (!strcmp(msg+lmsg-sizeof(EOFMARK)+1, EOFMARK)) {
      lua_pop(L, 1);
      return 1;
    }
  }
  return 0;
}

/*
** dojob is the CB reader for the input pipe and follows the calling convention
** for pipe reader CBs.  It has one argument: the stdin pipe that it is reading.
*/
static int dojob (lua_State *L) {
  size_t l;
  int status;
  const char *prompt;

//dbg_printf("dojob entered\n");
  lua_settop(L, 1);                                       /* pipe obj at S[1] */
  lua_pushlightfunction(L, &pipe_read);                  /* pobj:read at S[2] */
  lua_pushvalue(L, 1);                                    /* dup pobj to S[3] */
  lua_pushliteral(L, "\n+");                                  /* S[4] = "\n+" */
  lua_call(L, 2, 1);                               /* S[2] = pobj:read("\n+") */
  const char* b = lua_tolstring(L, 2, &l);         /* b = NULL if S[2] is nil */

  if ((lua_isnil(L, 2) || l == 0)) {
    /* If the pipe is empty then return false to suppress automatic reposting */
//dbg_printf("stdin empty\n");
    lua_pushboolean(L, false);
    return 1;                                                 /* return false */
  }

  if (b[l-1] != '\n') {
//dbg_printf("unreading part line\n");
    /* likewise if not CR terminated, then unread and ditto */
    lua_pushlightfunction(L, &pipe_unread);              /* pobj:read at S[1] */
    lua_insert(L, 1);
    lua_call(L, 2, 0);                                   /* pobj:unread(line) */
    lua_pushboolean(L, false);
    return 1;                                                 /* return false */
  } else {
//dbg_printf("popping CR terminated string(%d) %s", l-1,  b);
  }
 /*
  * Now we can process a proper CR terminated line
  */
  lua_pushlstring(L, b, --l);                                /* remove end CR */
  lua_remove(L, 2);
  b = lua_tostring(L, 2);

  if (MLref != LUA_NOREF) {
    /* processing multiline */
    lua_rawgeti(L, LUA_REGISTRYINDEX, MLref);         /* insert prev lines(s) */
    lua_pushliteral(L, "\n");                                    /* insert CR */
    lua_pushvalue(L, 2);                                      /* dup new line */
    lua_concat(L, 3);                                         /* concat all 3 */
    lua_remove(L, 2);                               /* and shift down to S[2] */
   } else if (b[0] == '=') {
    /* If firstline and of the format =<expression> */
    lua_pushfstring(L, "return %s", b+1);
    lua_remove(L, 2);
  }

  /* ToS is at S[2] which contains the putative chunk to be compiled */

  status = luaL_loadbuffer(L, lua_tostring(L, 2), lua_strlen(L, 2), "=stdin");

  if (incomplete(L, status)) {
    /* Store line back in the Reg mlref sot */
    if (MLref == LUA_NOREF) {
      MLref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      lua_rawseti(L, LUA_REGISTRYINDEX, MLref);
    }
  } else {
    /* compile finished OK or with hard error */
    lua_remove(L, 2);                    /* remove line because now redundant */
    if (MLref!= LUA_NOREF) {       /* also remove multiline if it exists */
      luaL_unref(L, LUA_REGISTRYINDEX, MLref);
      MLref= LUA_NOREF;
    }
    /* Execute the compiled chunk of successful */
    if (status == 0) {
      status = docall(L, 0, 0);
    }
    /* print any returned results or error message */
    if (status && !lua_isnil(L, -1))
      l_print(L, 1);
    if (status == 0 && lua_gettop(L) - 1)
      l_print(L, lua_gettop(L) - 1);

    lua_settop(L, 2);
    if (status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
  }

  prompt = get_prompt(L, MLref!= LUA_NOREF ? 0 : 1);
  input_setprompt(prompt);
  puts(prompt);

  lua_pushnil(L);
  return 1;                       /* return nil will retask if pipe not empty */
}


/*
 * Kick off library and UART input handling before opening the module
 * libraries.  Note that as this is all done within a Lua task, so error
 * handling is left to the Lua task traceback mechanism.
 */
extern void luaL_dbgbreak(void);

static int pmain (lua_State *L) {
  const char *init = LUA_INIT_STRING;
  globalL = L;

  lua_gc(L, LUA_GCSTOP, 0);                  /* stop GC during initialization */
  luaL_openlibs(L);                                         /* open libraries */
  lua_gc(L, LUA_GCRESTART, 0);                 /* restart GC and set EGC mode */
  legc_set_mode( L, EGC_ALWAYS, 4096 );
  lua_settop(L, 0);

  lua_pushliteral(L, "stdin");
  lua_pushlightfunction(L, &pipe_create);
  lua_pushlightfunction(L, &dojob);
  lua_pushinteger(L, LUA_TASK_LOW);
  lua_call(L, 2, 1);                /* ToS = pipe.create(dojob, low_priority) */
  lua_rawset(L, LUA_REGISTRYINDEX);   /* and stash input pipe in Reg["stdin"] */

  input_setup(LUA_MAXINPUT, get_prompt(L, 1));
  lua_input_string(" \n", 2);               /* queue CR to issue first prompt */
#if !defined(DISABLE_STARTUP_BANNER)
  print_version(L);
#endif

  /* and last of all, kick off application initialisation */
  if (init[0] == '@')
    dofile(L, init+1);
  else
    dostring(L, init, LUA_INIT);
  return 0;
}

/*
** The system initialisation CB nodemcu_init() calls lua_main() to startup
** the Lua environment by calling lua_open() which initiates the core Lua VM.
** The initialisation of the libraries, etc. is carried out by pmain in a
** separate Lua task, which also kicks off the user application through the
** LUA_INIT_STRING hook.
*/
void lua_main (void) {
  lua_State *L = lua_open();                                  /* create state */
  if (L == NULL) {
    l_message("cannot create state: not enough memory");
    return;
  }
  lua_pushlightfunction(L, &pmain);   /* Call 'pmain' as a high priority task */
  luaN_posttask(L, LUA_TASK_HIGH);
}

/*
** The Lua interpreter is event-driven and task-oriented in NodeMCU rather than
** based on a readline poll loop as in the standard implementation. Input lines
** can come from one of two sources: the application can "push" lines for the
** interpreter to compile and execute, or they can come from the UART. To
** minimise application blocking, the lines are queued in a pipe when received,
** with the Lua interpreter task attached to the pipe as its reader task. This
** CB processes one line of input per task execution.
**
** Even though lines can be emitted from independent sources (the UART and the
** node API), and they could in theory get interleaved, the strategy here is
** "let the programmer beware": interactive input will normally only occur in
** development and injected input occur in telnet type applications. If there
** is a need for interlocks, then the application should handle this.
*/
//static int n = 0;
void lua_input_string (const char *line, int len) {
  lua_State *L = globalL;
  lua_getfield(L, LUA_REGISTRYINDEX, "stdin");
  lua_rawgeti(L, -1, 1);                  /* get the pipe_write from stdin[1] */
  lua_insert(L, -2);                                  /* stick above the pipe */
  lua_pushlstring(L, line, len);

//const char*b = lua_tostring(L, -1);
//dbg_printf("Pushing (%u): %s", len, b); 
  lua_call(L, 2, 0);                                     /* stdin:write(line) */
}
