/*
** Lua stand-alone interpreter
** See Copyright Notice in lua.h
*/

#define lua_c

#define LUA_CORE
#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "user_version.h"
#include "driver/input.h"

#define lua_c

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lgc.h"
#include "lnodemcu.h"

#include "platform.h"


#if !defined(LUA_PROMPT)
#define LUA_PROMPT		"> "
#define LUA_PROMPT2		">> "
#endif


#ifndef LUA_INIT_STRING
#define LUA_INIT_STRING "@init.lua"
#endif

/*
** The NodeMCU version of lua.c is structurally different for standard lua.c as
** a result of architectural drivers arising from its context and being initiated
** within the startup sequence of an IoT SoC embedded runtime.
**
**  1) Processing is based on a single threaded event loop model (somewhat akin
**     to Node.js), so access to most system services is asyncronous and uses a
**     callback mechanism.  The Lua interactive mode processes input lines from
**     a stdin pipe within this framework.  Input must be handled on a line by
**     line basis and indeed other Lua tasks might interleave any multiline
**     processing here.  The doREPL approach doesn't work.
**
**  2) Most OS services and enviroment processing are supported so much of the
**     standard functionality is irrelevant and is stripped out for simplicity.
**
**  3) stderr and stdout redirection aren't offered as an OS service, so this
**     is handled in the baselib print function and errors are sent to print.
*/

static int pmain (lua_State *L);
static int dojob (lua_State *L);
void lua_input_string (const char *line, int len);


/*
** Prints (calling the Lua 'print' function) any values on the stack
*/
static void l_print (lua_State *L, int n) {
  if (n > 0) {  /* any result to be printed? */
    luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
    lua_getglobal(L, "print");
    lua_insert(L, -n-1);
    if (lua_pcall(L, n, 0, 0) != LUA_OK)
      lua_writestringerror( "error calling 'print' (%s)\n",
                            lua_tostring(L, -1));
  }
}


/*
** Message handler used to run all chunks
*/
static int msghandler (lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = lua_pushfstring(L, "(error object is a %s value)",
                               luaL_typename(L, 1));
  }
  luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}


/*
** Interface to 'lua_pcall', which sets appropriate message function
** and error handler. Used to run all chunks.
*/
static int docall (lua_State *L, int narg, int nres) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, msghandler);  /* push message handler */
  lua_insert(L, base);  /* put it under function and args */
  status = lua_pcall(L, narg, (nres ? 0 : LUA_MULTRET), base);
  lua_remove(L, base);  /* remove message handler from the stack */
  /* force a complete garbage collection in case of errors */
  if (status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
  return status;
}

static void print_version (lua_State *L) {
  lua_pushliteral (L, "\n" NODE_VERSION " build " BUILD_DATE
                      " powered by " LUA_RELEASE " on SDK ");
  lua_pushstring (L, SDK_VERSION);
  lua_concat (L, 2);
  l_print(L, 1);
}


/*
** Returns the string to be used as a prompt by the interpreter.
*/
static const char *get_prompt (lua_State *L, int firstline) {
  const char *p;
  lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2");
  p = lua_tostring(L, -1);
  if (p == NULL) p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
  lua_pop(L, 1);  /* remove global */
  return p;
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define MARKLEN		(sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete (lua_State *L, int status) {
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    if (lmsg >= MARKLEN && !strcmp(msg + lmsg - MARKLEN, EOFMARK)) {
      lua_pop(L, 1);
      return 1;
    }
  }
  return 0;
}

/*
** dojob replaces the standard doREPL loop.  If is configured as a CB reader
** from the stdin pipe, and follows the calling conventions for pipe reader
** CBs.  It has one argument, the stdin pipe that it is reading.
**
** Note that the Lua stack can't be used to stash part-line components as
** other C API and Lua functions might be executed as tasks between lines in
** a multiline, so a standard luaL_ref() registry entry is used instead.
*/
static int dojob (lua_State *L) {
  static int MLref = LUA_NOREF;
  size_t l;
  int status;
  const char *prompt;

  lua_settop(L, 1);                                       /* pipe obj at S[1] */
  lua_getfield(L, 1, "read");                            /* pobj:read at S[2] */
  lua_pushvalue(L, 1);                                    /* dup pobj to S[3] */
  lua_pushliteral(L, "\n+");                                  /* S[4] = "\n+" */
  lua_call(L, 2, 1);                               /* S[2] = pobj:read("\n+") */
  const char* b = lua_tolstring(L, 2, &l);         /* b = NULL if S[2] is nil */
 /*
  * If the pipe is empty, or the line not CR terminated, return false to
  * suppress automatic reposting
  */
  lua_pushboolean(L, false);
  if ((lua_isnil(L, 2) || l == 0))
    return 1;                                   /* return false if pipe empty */
  if (b[l-1] != '\n') {
    /* likewise if  then unread and ditto */
    lua_getfield(L, 1, "unread");
    lua_insert(L, 1);                    /* insert pipe.unread above the pipe */
    lua_call(L, 2, 0);                                   /* pobj:unread(line) */
    return 1;                                                 /* return false */
  }
  lua_pop(L, 1);                                    /* dump false value at ToS */
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
 /*
  * ToS is at S[2] which contains the putative chunk to be compiled
  */
  status = luaL_loadbuffer(L, lua_tostring(L, 2), lua_strlen(L, 2), "=stdin");

  if (incomplete(L, status)) {
    /* Store line back in the Reg mlref sot */
    if (MLref == LUA_NOREF)
      MLref = luaL_ref(L, LUA_REGISTRYINDEX);
    else
      lua_rawseti(L, LUA_REGISTRYINDEX, MLref);
  } else {
    /* compile finished OK or with hard error */
    lua_remove(L, 2);                    /* remove line because now redundant */
    if (MLref!= LUA_NOREF) {            /* also remove multiline if it exists */
      luaL_unref(L, LUA_REGISTRYINDEX, MLref);
      MLref = LUA_NOREF;
    }
    /* Execute the compiled chunk of successful */
    if (status == 0)
      status = docall(L, 0, 0);
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
  lua_writestring(prompt,strlen(prompt));       /* Note prompt NOT REDIRECTED */
  lua_pushnil(L);
  return 1;                       /* return nil will retask if pipe not empty */
}


/*
** Main body of stand-alone interpreter (called as a posted task).
*/
extern void system_restart(void);
static int pmain (lua_State *L) {
  const char *init = (char *) lua_touserdata(L, 1);
  int status;
  lua_gc(L, LUA_GCSTOP, 0);                  /* stop GC during initialization */
  luaL_openlibs(L);    /* the nodemcu open will throw to signal an LFS reload */
  lua_gc( L, LUA_GCSETMEMLIMIT, 4096 );
  lua_gc(L, LUA_GCRESTART, 0);                 /* restart GC and set EGC mode */
  lua_settop(L, 0);
  lua_pushliteral(L, "stdin");
  lua_getglobal(L, "pipe");
  lua_getfield(L, -1, "create");
  lua_remove(L, -2);
  lua_pushcfunction(L, dojob);
  lua_pushinteger(L, LUA_TASK_LOW);
  lua_call(L, 2, 1);                /* ToS = pipe.create(dojob, low_priority) */
  lua_rawset(L, LUA_REGISTRYINDEX);   /* and stash input pipe in Reg["stdin"] */

  input_setup(LUA_MAXINPUT, get_prompt(L, 1));
  lua_input_string(" \n", 2);               /* queue CR to issue first prompt */
  print_version(L);
 /*
  * And last of all, kick off application initialisation.  Note that if
  * LUA_INIT_STRING is a file reference and the file system is uninitialised
  * then attempting the open will trigger a file system format.
  */
  if (init[0] == '@')
    status = luaL_loadfile(L, init+1);
  else
    status = luaL_loadbuffer(L, init, strlen(init), "=INIT");
  if (status == LUA_OK)
    status = docall(L, 0, 0);
  if (status != LUA_OK)
    l_print (L, 1);
  return 0;
}

void lua_main (void);
static int cpmain (lua_State *L) {
  const char *RCRinit = NULL;
  uint32_t n = platform_rcr_read(PLATFORM_RCR_INITSTR, cast(void**, &RCRinit));
  const char *init = RCRinit ? RCRinit : LUA_INIT_STRING;
  int status;
  UNUSED(n);
  lua_pushcfunction(L, pmain);
  lua_pushlightuserdata(L, cast(void *,init));
  status = lua_pcall(L, 1, 1, 0);
  if (status != LUA_OK && (lua_isboolean(L,-1) && lua_toboolean(L,-1))) {
   /*
    * An LFS image has been loaded so close and restart the RTS
    */
    luaL_posttask(NULL, LUA_TASK_HIGH+1);  /* NULL/high+1 restarts lua_main() */
    return 0;
  }
  return lua_error(L);           /* rethrow the error for post task to report */
}

/*
** The system initialisation CB nodemcu_init() calls lua_main() to startup the
** Lua environment by calling luaL_newstate() which initiates the core Lua VM.
** The initialisation of the libraries, etc. is carried out by pmain in a
** separate Lua task, which also kicks off the user application through the
** LUA_INIT_STRING hook.
*/
void lua_main (void) {
  lua_State *L = lua_getstate();
  if (L)
    lua_close(L);
  L = luaL_newstate();  /* create state */
  if (L == NULL) {
    lua_writestringerror( "cannot create state: %s", "not enough memory");
    return;
  }
  lua_pushcfunction(L, cpmain); /* Call 'pmain' wrapper as a high priority task */
  luaL_posttask(L, LUA_TASK_HIGH);
}


void lua_input_string (const char *line, int len) {
  lua_State *L = lua_getstate();
  lua_getfield(L, LUA_REGISTRYINDEX, "stdin");
  lua_rawgeti(L, -1, 1);                  /* get the pipe_write from stdin[1] */
  lua_insert(L, -2);                                  /* stick above the pipe */
  lua_pushlstring(L, line, len);
  lua_call(L, 2, 0);                                     /* stdin:write(line) */
}
