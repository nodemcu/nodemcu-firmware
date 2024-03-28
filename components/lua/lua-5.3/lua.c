/*
** NodeMCU Lua 5.1 and 5.3 main initiator and comand interpreter
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "user_version.h"
#include "linput.h"

#define lua_c
#define LUA_CORE

#ifndef LUA_VERSION_51  /* LUA_VERSION_NUM == 503 */
#define LUA_VERSION_53
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lprefix.h"
#include "lgc.h"
#include "lnodemcu.h"
#endif

#include "platform.h"

#if !defined(LUA_PROMPT)
#define LUA_PROMPT		"> "
#define LUA_PROMPT2		">> "
#endif

#ifndef LUA_INIT_STRING
# define LUA_INIT_STRING CONFIG_LUA_INIT_STRING
#endif

#if !defined(STARTUP_COUNT)
# define STARTUP_COUNT
#endif

/*
** The NodeMCU version of lua.c is structurally different for standard lua.c
** as a result of architectural drivers arising from its context and being
** initiated within the startup sequence of an IoT SoC embedded runtime.
**
**  1) Processing is based on a single threaded event loop model (somewhat akin
**     to Node.js), so access to most system services is asyncronous and uses
**     a callback mechanism.  The Lua interactive mode processes input lines
**     that are provided by the firmware on a line by line basis and indeed
**     other Lua tasks might interleave any multiline processing, so the
**     standard doREPL approach won't work.
**
**  2) Most OS services and enviroment processing are supported so much of the
**     standard functionality is irrelevant and is stripped out for simplicity.
**
**  3) stderr and stdout redirection aren't offered as an OS service, so this
**     is handled in the baselib print function and errors are sent to print.
*/
lua_State *globalL = NULL;

static int pmain (lua_State *L);
void lua_input_string (const char *line, int len);

/*
** Prints (calling the Lua 'print' function) to print n values on the stack
*/
static void l_print (lua_State *L, int n) {
  if (n > 0) {  /* any result to be printed? */
    luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
    lua_getglobal(L, "print");
    lua_insert(L, -n-1);
    if (lua_pcall(L, n, 0, 0) != LUA_OK) {
      lua_writestringerror("error calling 'print' (%s)\n", lua_tostring(L, -1));
      lua_settop(L, -n-1);
    }
  }
}


/*
** Message handler is used with all chunks calls. Returns the traceback on ToS
*/
static int msghandler (lua_State *L) {
  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "traceback");
  if (lua_isfunction(L, -1)) {
    lua_insert(L, 1);                 /* insert tracback function above error */
    lua_pop(L, 1);                              /* dump the debug table entry */
    lua_pushinteger(L, 2);                /* skip this function and traceback */
    lua_call(L, 2, 1);      /* call debug.traceback and return it as a string */
  }
  return 1;  /* return the traceback */
}


/*
** Interface to 'lua_pcall', which sets appropriate message function and error 
** handler. Used to run all chunks.  Results or error traceback left on stack.
** This function is interactive so unlike lua_pcallx(), the error is sent direct
** to the print function and erroring does not trigger an on error restart. 
*/
static int docall (lua_State *L, int narg) {
  int status;
  int base = lua_gettop(L) - narg;                          /* function index */
  lua_pushcfunction(L, msghandler);                   /* push message handler */
  lua_insert(L, base);                         /* put it under chunk and args */
  status = lua_pcall(L, narg, LUA_MULTRET, base);
  lua_remove(L, base);               /* remove message handler from the stack */
  /* force a complete garbage collection in case of errors */
  if (status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
  return status;
}


#if !defined(DISABLE_STARTUP_BANNER) && defined(ESP8266)
static void print_version (lua_State *L) {
  lua_writestringerror( "\n" NODE_VERSION " build " BUILD_DATE
                        " powered by " LUA_RELEASE " on SDK %s\n", SDK_VERSION);
}
#endif

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


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
#ifdef LUA_VERSION_51
#define EOFMARK   LUA_QL("<eof>")
#else
#define EOFMARK  "<eof>"
#endif
#define MARKLEN  (sizeof(EOFMARK)/sizeof(char) - 1)
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

static void l_create_stdin (lua_State *L);
/*
** Note that the Lua stack can't be used to stash part-line components as
** other C API and Lua functions might be executed as tasks between lines in
** a multiline, so a standard luaL_ref() registry entry is used instead.
*/
static void dojob (lua_State *L) {
  static int MLref = LUA_NOREF;        /* Lua Reg entry for cached multi-line */
  int status;
  const char *prompt;
  size_t l;
  const char *b = lua_tostring(L, -1);        /* ToS contains next input line */

  if (MLref != LUA_NOREF) {
    /* processing multiline */
    lua_rawgeti(L, LUA_REGISTRYINDEX, MLref);         /* insert prev lines(s) */
    lua_pushliteral(L, "\n");                                    /* insert CR */
    lua_pushvalue(L, -3);                                     /* dup new line */
    lua_concat(L, 3);                                         /* concat all 3 */
    lua_remove(L,-2);                                /* and shift down to ToS */
   } else if (b[0] == '=') {  /* If firstline and of the format =<expression> */
    lua_pushfstring(L, "return %s", b+1);
    lua_remove(L, -2);
  }  
 /*
  * ToS is at S[2] which contains the putative chunk to be compiled
  */
  b = lua_tolstring(L, -1, &l);
  status = luaL_loadbuffer(L, b, l, "=stdin");
  if (incomplete(L, status)) {
    /* Store line back in the Reg mlref sot */
    if (MLref == LUA_NOREF) {
      MLref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      lua_rawseti(L, LUA_REGISTRYINDEX, MLref);
    }
  } else {
    /* compile finished OK or with hard error */
    lua_remove(L, -2);                   /* remove line because now redundant */
    if (MLref != LUA_NOREF) {           /* also remove multiline if it exists */
      luaL_unref(L, LUA_REGISTRYINDEX, MLref);
      MLref = LUA_NOREF;
    }
    /* Execute the compiled chunk of successful */
    if (status == 0)
      status = docall(L, 0);
    /* print any returned results or error message */
    if (status && !lua_isnil(L, -1)) {
      lua_pushliteral(L, "Lua error: ");
      lua_insert(L , -2);
    }
    l_print(L, lua_gettop(L) - 1);        /* print error or results one stack */
  }

  prompt = get_prompt(L, MLref!= LUA_NOREF ? 0 : 1);
  input_setprompt(prompt);
  lua_writestring(prompt,strlen(prompt));
  fsync(fileno(stdout)); /* work around IDF issue on ACM consoles */
}


/*
** Main body of standalone interpreter.
*/
static int pmain (lua_State *L) {
  const char *init = LUA_INIT_STRING;
  int status;
  STARTUP_COUNT;

  lua_gc(L, LUA_GCSTOP, 0);                  /* stop GC during initialization */
  luaL_openlibs(L);        /* Nodemcu open will throw to signal an LFS reload */
#ifdef LUA_VERSION_51
  lua_setegcmode( L, EGC_ALWAYS, 4096 );
#else
  lua_gc( L, LUA_GCSETMEMLIMIT, 4096 );
#endif
  lua_gc(L, LUA_GCRESTART, 0);                 /* restart GC and set EGC mode */
  lua_settop(L, 0);
  l_create_stdin(L);
  input_setup(LUA_MAXINPUT, get_prompt(L, 1));
  lua_input_string(" \n", 2);               /* queue CR to issue first prompt */

#if defined(LUA_USE_ESP8266) && !defined(DISABLE_STARTUP_BANNER)
  if ((platform_rcr_get_startup_option() & STARTUP_OPTION_NO_BANNER) == 0) {
    print_version(L);
  }
#else
  printf("\n%s build %s powered by %s [%s] on IDF %s\n",
    NODE_VERSION, BUILD_DATE, LUA_RELEASE, XLUA_OPT_STR, IDF_VER);
#endif
 /*
  * And last of all, kick off application initialisation.  Note that if
  * LUA_INIT_STRING is a file reference and the file system is uninitialised
  * then attempting the open will trigger a file system format.
  */
#if defined(LUA_USE_ESP8266)
  platform_rcr_read(PLATFORM_RCR_INITSTR, (void**) &init);
#endif
  STARTUP_COUNT;
  if (init[0] == '!') { /* !module is a compile-free way of executing LFS module */
    luaL_pushlfsmodule(L);
    lua_pushstring(L, init+1);
    lua_call(L, 1, 1);  /* return LFS.module or nil */
    status = LUA_OK;
    if (!lua_isfunction(L, -1)) {
      lua_pushfstring(L, "cannot load LFS.%s", init+1);
      status = LUA_ERRRUN;
    }
  } else {
    status = (init[0] == '@') ?
             luaL_loadfile(L, init+1) :
             luaL_loadbuffer(L, init, strlen(init), "=INIT");
  }
  STARTUP_COUNT;
  if (status == LUA_OK)
    status = docall(L, 0);
  if (status != LUA_OK)
    l_print (L, 1);
  STARTUP_COUNT;
  return 0;
}


/*
** The system initialisation CB nodemcu_init() calls lua_main() to startup the
** Lua environment by calling luaL_newstate() which initiates the core Lua VM.
** The initialisation of the libraries, etc. can potentially throw errors and
** so is wrapped in a protected call which also kicks off the user application
** through the LUA_INIT_STRING hook.
*/
int lua_main (void) {
  lua_State *L = luaL_newstate();
  if (L == NULL) {
    lua_writestringerror( "cannot create state: %s", "not enough memory");
    return 0;
  }
  globalL = L;
  lua_pushcfunction(L, pmain);
  if (docall(L, 0) != LUA_OK) {
    if (strstr(lua_tostring(L, -1),"!LFSrestart!")) {
      lua_close(L);
      return 1;                         /* non-zero return to flag LFS reload */
    }
    l_print(L, 1);
  }
  return 0;
}


lua_State *lua_getstate(void) {
  return globalL;
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

void lua_input_string (const char *line, int len) {
  lua_State *L = globalL;
  lua_getfield(L, LUA_REGISTRYINDEX, "stdin");
  lua_rawgeti(L, -1, 1);                  /* get the pipe_write from stdin[1] */
  lua_insert(L, -2);                                  /* stick above the pipe */
  lua_pushlstring(L, line, len);
  lua_call(L, 2, 0);                                     /* stdin:write(line) */
}

/*
** CB reader for the stdin pipe, and follows the calling conventions for a
** pipe readers; it has one argument, the stdin pipe that it is reading.
*/
static int l_read_stdin (lua_State *L) {
  size_t l;
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
    /* likewise if not CR terminated, then unread and ditto */
    lua_insert(L, 1);                   /* insert false return above the pipe */
    lua_getfield(L, 2, "unread");
    lua_insert(L, 2);                    /* insert pipe.unread above the pipe */
    lua_call(L, 2, 0);                                   /* pobj:unread(line) */
    return 1;                                                 /* return false */
  }
  lua_pop(L, 1);                                   /* dump false value at ToS */
 /*
  * Now we can process a proper CR terminated line
  */
  lua_pushlstring(L, b, --l);                                /* remove end CR */
  lua_remove(L, 2);
  dojob(L);
  return 0;
}


/*
** Create and initialise the stdin pipe
*/
static void l_create_stdin (lua_State *L) {
  lua_pushliteral(L, "stdin");
  lua_getglobal(L, "pipe");
  lua_getfield(L, -1, "create");
  lua_remove(L, -2);
  lua_pushcfunction(L, l_read_stdin);
  lua_pushinteger(L, LUA_TASK_LOW);
  lua_call(L, 2, 1);                /* ToS = pipe.create(dojob, low_priority) */
  lua_rawset(L, LUA_REGISTRYINDEX);   /* and stash input pipe in Reg["stdin"] */
}
