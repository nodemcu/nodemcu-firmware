/*
** $Id: liolib.c,v 2.73.1.4 2010/05/14 15:33:51 roberto Exp $
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/


// #include "c_errno.h"
#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "vfs.h"

#define liolib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"


#define IO_INPUT	1
#define IO_OUTPUT	2
#define IO_STDERR	0

#if LUA_OPTIMIZE_MEMORY != 2
#define LUA_IO_GETFIELD(f)	lua_rawgeti(L, LUA_ENVIRONINDEX, f)
#define LUA_IO_SETFIELD(f)  lua_rawseti(L, LUA_ENVIRONINDEX, f)
#else
#define LUA_IO_GETFIELD(f)  lua_rawgeti(L, LUA_REGISTRYINDEX, liolib_keys[f])
#define LUA_IO_SETFIELD(f)  lua_rawseti(L, LUA_REGISTRYINDEX, liolib_keys[f])

/* "Pseudo-random" keys for the registry */
static const int liolib_keys[] = {(int)&luaL_callmeta, (int)&luaL_typerror, (int)&luaL_argerror};
#endif

static const char *const fnames[] = {"input", "output"};

static int pushresult (lua_State *L, int i, const char *filename) {
  int en = vfs_ferrno(0);  /* calls to Lua API may change this value */
  if (i) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushnil(L);
    if (filename)
      lua_pushfstring(L, "%s: err(%d)", filename, en);
    else
      lua_pushfstring(L, "err(%d)", en);
    lua_pushinteger(L, en);
    return 3;
  }
}


static void fileerror (lua_State *L, int arg, const char *filename) {
  lua_pushfstring(L, "%s: err(%d)", filename, vfs_ferrno(0));
  luaL_argerror(L, arg, lua_tostring(L, -1));
}


#define tofilep(L)	((int *)luaL_checkudata(L, 1, LUA_FILEHANDLE))


static int io_type (lua_State *L) {
  void *ud;
  luaL_checkany(L, 1);
  ud = lua_touserdata(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_FILEHANDLE);
  if (ud == NULL || !lua_getmetatable(L, 1) || !lua_rawequal(L, -2, -1))
    lua_pushnil(L);  /* not a file */
  else if (*((int *)ud) < FS_OPEN_OK)
    lua_pushliteral(L, "closed file");
  else
    lua_pushliteral(L, "file");
  return 1;
}


static int tofile (lua_State *L) {
  int *f = tofilep(L);
  if (*f < FS_OPEN_OK)
    luaL_error(L, "attempt to use a closed file");
  return *f;
}



/*
** When creating file handles, always creates a `closed' file handle
** before opening the actual file; so, if there is a memory error, the
** file is not left opened.
*/
static int *newfile (lua_State *L) {
  int *pf = (int *)lua_newuserdata(L, sizeof(int));
  *pf = FS_OPEN_OK - 1;  /* file handle is currently `closed' */
  luaL_getmetatable(L, LUA_FILEHANDLE);
  lua_setmetatable(L, -2);
  return pf;
}


#if LUA_OPTIMIZE_MEMORY != 2
/*
** function to (not) close the standard files stdin, stdout, and stderr
*/
static int io_noclose (lua_State *L) {
  lua_pushnil(L);
  lua_pushliteral(L, "cannot close standard file");
  return 2;
}

#if 0
/*
** function to close 'popen' files
*/
static int io_pclose (lua_State *L) {
  int *p = tofilep(L);
  int ok = lua_pclose(L, *p);
  *p = FS_OPEN_OK - 1;
  return pushresult(L, ok, NULL);
}
#endif

/*
** function to close regular files
*/
static int io_fclose (lua_State *L) {
  int *p = tofilep(L);
  int ok = (vfs_close(*p) == 0);
  *p = FS_OPEN_OK - 1;
  return pushresult(L, ok, NULL);
}
#endif

static int aux_close (lua_State *L) {
#if LUA_OPTIMIZE_MEMORY != 2
  lua_getfenv(L, 1);
  lua_getfield(L, -1, "__close");
  return (lua_tocfunction(L, -1))(L);
#else
  int *p = tofilep(L);
  if(*p == c_stdin || *p == c_stdout || *p == c_stderr)
  {
    lua_pushnil(L);
    lua_pushliteral(L, "cannot close standard file");
    return 2;  
  }
  int ok = (vfs_close(*p) == 0);
  *p = FS_OPEN_OK - 1;
  return pushresult(L, ok, NULL);
#endif 
}


static int io_close (lua_State *L) {
  if (lua_isnone(L, 1))
    LUA_IO_GETFIELD(IO_OUTPUT);
  tofile(L);  /* make sure argument is a file */
  return aux_close(L);
}


static int io_gc (lua_State *L) {
  int f = *tofilep(L);
  /* ignore closed files */
  if (f != FS_OPEN_OK - 1)
    aux_close(L);
  return 0;
}


static int io_tostring (lua_State *L) {
  int f = *tofilep(L);
  if (f == FS_OPEN_OK - 1)
    lua_pushliteral(L, "file (closed)");
  else
    lua_pushfstring(L, "file (%i)", f);
  return 1;
}


static int io_open (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  const char *mode = luaL_optstring(L, 2, "r");
  int *pf = newfile(L);
  *pf = vfs_open(filename, mode);
  return (*pf == FS_OPEN_OK - 1) ? pushresult(L, 0, filename) : 1;
}


/*
** this function has a separated environment, which defines the
** correct __close for 'popen' files
*/
#if 0
static int io_popen (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  const char *mode = luaL_optstring(L, 2, "r");
  int *pf = newfile(L);
  *pf = lua_popen(L, filename, fs_mode2flags(mode));
  return (*pf == FS_OPEN_OK - 1) ? pushresult(L, 0, filename) : 1;
}


static int io_tmpfile (lua_State *L) {
  int *pf = newfile(L);
  *pf = tmpfile();
  return (*pf == FS_OPEN_OK - 1) ? pushresult(L, 0, NULL) : 1;
}
#endif

static int getiofile (lua_State *L, int findex) {
  int *pf;
  LUA_IO_GETFIELD(findex);
  pf = (int *)lua_touserdata(L, -1);
  if (pf == NULL || *pf == FS_OPEN_OK - 1){
    luaL_error(L, "default %s file is closed", fnames[findex - 1]);
    return FS_OPEN_OK - 1;
  }
  return *pf;
}


static int g_iofile (lua_State *L, int f, const char *mode) {
  if (!lua_isnoneornil(L, 1)) {
    const char *filename = lua_tostring(L, 1);
    if (filename) {
      int *pf = newfile(L);
      *pf = vfs_open(filename, mode);
      if (*pf == FS_OPEN_OK - 1)
        fileerror(L, 1, filename);
    }
    else {
      tofile(L);  /* check that it's a valid file handle */
      lua_pushvalue(L, 1);
    }
    LUA_IO_SETFIELD(f);
  }
  /* return current value */
  LUA_IO_GETFIELD(f);
  return 1;
}


static int io_input (lua_State *L) {
  return g_iofile(L, IO_INPUT, "r");
}


static int io_output (lua_State *L) {
  return g_iofile(L, IO_OUTPUT, "w");
}


static int io_readline (lua_State *L);


static void aux_lines (lua_State *L, int idx, int toclose) {
  lua_pushvalue(L, idx);
  lua_pushboolean(L, toclose);  /* close/not close file when finished */
  lua_pushcclosure(L, io_readline, 2);
}


static int f_lines (lua_State *L) {
  tofile(L);  /* check that it's a valid file handle */
  aux_lines(L, 1, 0);
  return 1;
}


static int io_lines (lua_State *L) {
  if (lua_isnoneornil(L, 1)) {  /* no arguments? */
    /* will iterate over default input */
    LUA_IO_GETFIELD(IO_INPUT);
    return f_lines(L);
  }
  else {
    const char *filename = luaL_checkstring(L, 1);
    int *pf = newfile(L);
    *pf = vfs_open(filename, "r");
    if (*pf == FS_OPEN_OK - 1)
      fileerror(L, 1, filename);
    aux_lines(L, lua_gettop(L), 1);
    return 1;
  }
}


/*
** {======================================================
** READ
** =======================================================
*/

#if 0
static int read_number (lua_State *L, int f) {
  lua_Number d;
  if (fs_scanf(f, LUA_NUMBER_SCAN, &d) == 1) {
    lua_pushnumber(L, d);
    return 1;
  }
  else {
    lua_pushnil(L);  /* "result" to be removed */
    return 0;  /* read fails */
  }
}
#endif

static int test_eof (lua_State *L, int f) {
  int c = vfs_getc(f);
  vfs_ungetc(c, f);
  lua_pushlstring(L, NULL, 0);
  return (c != EOF);
}

#if 0
static int read_line (lua_State *L, int f) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (;;) {
    size_t l;
    char *p = luaL_prepbuffer(&b);
    if (fs_gets(p, LUAL_BUFFERSIZE, f) == NULL) {  /* eof? */
      luaL_pushresult(&b);  /* close buffer */
      return (lua_objlen(L, -1) > 0);  /* check whether read something */
    }
    l = c_strlen(p);
    if (l == 0 || p[l-1] != '\n')
      luaL_addsize(&b, l);
    else {
      luaL_addsize(&b, l - 1);  /* do not include `eol' */
      luaL_pushresult(&b);  /* close buffer */
      return 1;  /* read at least an `eol' */
    }
  }
}
#else
static int read_line (lua_State *L, int f) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  signed char c;
  do {
    c = (signed char)vfs_getc(f);
    if (c==EOF) {
      break;
    }
    if (c != '\n') {
      luaL_addchar(&b, c);
    }
  } while (c != '\n');

  luaL_pushresult(&b);  /* close buffer */
  return (lua_objlen(L, -1) > 0);  /* check whether read something */
}
#endif

static int read_chars (lua_State *L, int f, size_t n) {
  size_t rlen;  /* how much to read */
  size_t nr;  /* number of chars actually read */
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  rlen = LUAL_BUFFERSIZE;  /* try to read that much each time */
  do {
    char *p = luaL_prepbuffer(&b);
    if (rlen > n) rlen = n;  /* cannot read more than asked */
    nr = vfs_read(f, p, rlen);
    luaL_addsize(&b, nr);
    n -= nr;  /* still have to read `n' chars */
  } while (n > 0 && nr == rlen);  /* until end of count or eof */
  luaL_pushresult(&b);  /* close buffer */
  return (n == 0 || lua_objlen(L, -1) > 0);
}


static int g_read (lua_State *L, int f, int first) {
  int nargs = lua_gettop(L) - 1;
  int success;
  int n;
  //vfs_clearerr(f);
  if (nargs == 0) {  /* no arguments? */
    success = read_line(L, f);
    n = first+1;  /* to return 1 result */
  }
  else {  /* ensure stack space for all results and for auxlib's buffer */
    luaL_checkstack(L, nargs+LUA_MINSTACK, "too many arguments");
    success = 1;
    for (n = first; nargs-- && success; n++) {
      if (lua_type(L, n) == LUA_TNUMBER) {
        size_t l = (size_t)lua_tointeger(L, n);
        success = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
      }
      else {
        const char *p = lua_tostring(L, n);
        luaL_argcheck(L, p && p[0] == '*', n, "invalid option");
        switch (p[1]) {
#if 0
          case 'n':  /* number */
            success = read_number(L, f);
            break;
#endif
          case 'l':  /* line */
            success = read_line(L, f);
            break;
          case 'a':  /* file */
            read_chars(L, f, ~((size_t)0));  /* read MAX_SIZE_T chars */
            success = 1; /* always success */
            break;
          default:
            return luaL_argerror(L, n, "invalid format");
        }
      }
    }
  }
  if (vfs_ferrno(f))
    return pushresult(L, 0, NULL);
  if (!success) {
    lua_pop(L, 1);  /* remove last result */
    lua_pushnil(L);  /* push nil instead */
  }
  return n - first;
}


static int io_read (lua_State *L) {
  return g_read(L, getiofile(L, IO_INPUT), 1);
}


static int f_read (lua_State *L) {
  return g_read(L, tofile(L), 2);
}


static int io_readline (lua_State *L) {
  int *pf = (int *)lua_touserdata(L, lua_upvalueindex(1));
  int sucess;
  if (pf == NULL || *pf == FS_OPEN_OK - 1){  /* file is already closed? */
    luaL_error(L, "file is already closed");
    return 0;
  }
  sucess = read_line(L, *pf);
  if (vfs_ferrno(*pf))
    return luaL_error(L, "err(%d)", vfs_ferrno(*pf));
  if (sucess) return 1;
  else {  /* EOF */
    if (lua_toboolean(L, lua_upvalueindex(2))) {  /* generator created file? */
      lua_settop(L, 0);
      lua_pushvalue(L, lua_upvalueindex(1));
      aux_close(L);  /* close it */
    }
    return 0;
  }
}

/* }====================================================== */


static int g_write (lua_State *L, int f, int arg) {
  int nargs = lua_gettop(L) - 1;
  int status = 1;
  for (; nargs--; arg++) {
#if 0
    if (lua_type(L, arg) == LUA_TNUMBER) {
      /* optimization: could be done exactly as for strings */
      status = status &&
          fs_printf(f, LUA_NUMBER_FMT, lua_tonumber(L, arg)) > 0;
    }
    else 
#endif
    {
      size_t l;
      const char *s = luaL_checklstring(L, arg, &l);
      status = status && (vfs_write(f, s, l) == l);
    }
  }
  return pushresult(L, status, NULL);
}


static int io_write (lua_State *L) {
  return g_write(L, getiofile(L, IO_OUTPUT), 1);
}


static int f_write (lua_State *L) {
  return g_write(L, tofile(L), 2);
}


static int f_seek (lua_State *L) {
  static const int mode[] = {VFS_SEEK_SET, VFS_SEEK_CUR, VFS_SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  int f = tofile(L);
  int op = luaL_checkoption(L, 2, "cur", modenames);
  long offset = luaL_optlong(L, 3, 0);
  op = vfs_lseek(f, offset, mode[op]);
  if (op)
    return pushresult(L, 0, NULL);  /* error */
  else {
    lua_pushinteger(L, vfs_tell(f));
    return 1;
  }
}

#if 0
static int f_setvbuf (lua_State *L) {
  static const int mode[] = {_IONBF, _IOFBF, _IOLBF};
  static const char *const modenames[] = {"no", "full", "line", NULL};
  int f = tofile(L);
  int op = luaL_checkoption(L, 2, NULL, modenames);
  lua_Integer sz = luaL_optinteger(L, 3, LUAL_BUFFERSIZE);
  int res = setvbuf(f, NULL, mode[op], sz);
  return pushresult(L, res == 0, NULL);
}
#endif


static int io_flush (lua_State *L) {
  return pushresult(L, vfs_flush(getiofile(L, IO_OUTPUT)) == 0, NULL);
}


static int f_flush (lua_State *L) {
  return pushresult(L, vfs_flush(tofile(L)) == 0, NULL);
}

#undef MIN_OPT_LEVEL
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
#if LUA_OPTIMIZE_MEMORY == 2
const LUA_REG_TYPE iolib_funcs[] = {
#else
const LUA_REG_TYPE iolib[] = {
#endif
  {LSTRKEY("close"), LFUNCVAL(io_close)},
  {LSTRKEY("flush"), LFUNCVAL(io_flush)},
  {LSTRKEY("input"), LFUNCVAL(io_input)},
  {LSTRKEY("lines"), LFUNCVAL(io_lines)},
  {LSTRKEY("open"), LFUNCVAL(io_open)},
  {LSTRKEY("output"), LFUNCVAL(io_output)},
  // {LSTRKEY("popen"), LFUNCVAL(io_popen)},
  {LSTRKEY("read"), LFUNCVAL(io_read)},
  // {LSTRKEY("tmpfile"), LFUNCVAL(io_tmpfile)},
  {LSTRKEY("type"), LFUNCVAL(io_type)},
  {LSTRKEY("write"), LFUNCVAL(io_write)},
  {LNILKEY, LNILVAL}
};

#if LUA_OPTIMIZE_MEMORY == 2
static int luaL_index(lua_State *L)
{
  return luaR_findfunction(L, iolib_funcs);
}
  
const luaL_Reg iolib[] = {
  {"__index", luaL_index},
  {NULL, NULL}
};
#endif

#undef MIN_OPT_LEVEL
#define MIN_OPT_LEVEL 1
#include "lrodefs.h"
const LUA_REG_TYPE flib[] = {
  {LSTRKEY("close"), LFUNCVAL(io_close)},
  {LSTRKEY("flush"), LFUNCVAL(f_flush)},
  {LSTRKEY("lines"), LFUNCVAL(f_lines)},
  {LSTRKEY("read"), LFUNCVAL(f_read)},
  {LSTRKEY("seek"), LFUNCVAL(f_seek)},
  // {LSTRKEY("setvbuf"), LFUNCVAL(f_setvbuf)},
  {LSTRKEY("write"), LFUNCVAL(f_write)},
  {LSTRKEY("__gc"), LFUNCVAL(io_gc)},
  {LSTRKEY("__tostring"), LFUNCVAL(io_tostring)},
#if LUA_OPTIMIZE_MEMORY > 0
  {LSTRKEY("__index"), LROVAL(flib)},
#endif
  {LNILKEY, LNILVAL}
};

static void createmeta (lua_State *L) {
#if LUA_OPTIMIZE_MEMORY == 0
  luaL_newmetatable(L, LUA_FILEHANDLE);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, flib);  /* file methods */
#else
  luaL_rometatable(L, LUA_FILEHANDLE, (void*)flib);  /* create metatable for file handles */
#endif
}


static void createstdfile (lua_State *L, int f, int k, const char *fname) {
  *newfile(L) = f;
#if LUA_OPTIMIZE_MEMORY != 2
  if (k > 0) {
    lua_pushvalue(L, -1);
    lua_rawseti(L, LUA_ENVIRONINDEX, k);
  }
  lua_pushvalue(L, -2);  /* copy environment */
  lua_setfenv(L, -2);  /* set it */
  lua_setfield(L, -3, fname);
#else
  lua_pushvalue(L, -1);
  lua_rawseti(L, LUA_REGISTRYINDEX, liolib_keys[k]);
  lua_setfield(L, -2, fname);
#endif
}

#if LUA_OPTIMIZE_MEMORY != 2
static void newfenv (lua_State *L, lua_CFunction cls) {
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, cls);
  lua_setfield(L, -2, "__close");
}
#endif

LUALIB_API int luaopen_io (lua_State *L) {
  createmeta(L);
#if LUA_OPTIMIZE_MEMORY != 2
  /* create (private) environment (with fields IO_INPUT, IO_OUTPUT, __close) */
  newfenv(L, io_fclose);
  lua_replace(L, LUA_ENVIRONINDEX);
  /* open library */
  luaL_register(L, LUA_IOLIBNAME, iolib);
  newfenv(L, io_noclose);  /* close function for default files */
#else
  luaL_register_light(L, LUA_IOLIBNAME, iolib);
  lua_pushvalue(L, -1);
  lua_setmetatable(L, -2);
#endif
#if 0
  /* create (and set) default files */
  createstdfile(L, c_stdin, IO_INPUT, "stdin");
  createstdfile(L, c_stdout, IO_OUTPUT, "stdout");
  createstdfile(L, c_stderr, IO_STDERR, "stderr");

#if LUA_OPTIMIZE_MEMORY != 2
  lua_pop(L, 1);  /* pop environment for default files */
  lua_getfield(L, -1, "popen");
  newfenv(L, io_pclose);  /* create environment for 'popen' */
  lua_setfenv(L, -2);  /* set fenv for 'popen' */
  lua_pop(L, 1);  /* pop 'popen' */
#endif
#endif
  return 1;
}
