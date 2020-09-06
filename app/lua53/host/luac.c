/*
 ** $Id: luac.c,v 1.76 2018/06/19 01:32:02 lhf Exp $
 ** Lua compiler (saves bytecodes to files; also lists bytecodes)
 ** See Copyright Notice in lua.h
 */

#define luac_c
#define LUA_CORE

#include "lprefix.h"

#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"
#include "lualib.h"       // ??
#include "ldebug.h"       // ??
#include "lnodemcu.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"      // ???
#include "lundump.h"

static void PrintFunction(const Proto *f, int full);
#define luaU_print PrintFunction

#define PROGNAME "luac.cross" /* default program name */ 
#define OUTPUT PROGNAME ".out" /* default output file */

static int listing = 0;             /* list bytecodes? */
static int dumping = 1;             /* dump bytecodes? */
static int stripping = 0;           /* strip debug information? */
static char Output[] = { OUTPUT };  /* default output file name */
static const char *output = Output; /* actual output file name */
static const char *progname = PROGNAME; /* actual program name */
static lu_int32 address = 0;        /* output flash image at absolute location */
static lu_int32 maxSize = 0x40000;  /* maximuum uncompressed image size */
static const char *execute;         /* executed a Lua file */
char *LFSimageName;


#define IROM0_SEG    0x40200000ul
#define IROM0_SEGMAX 0x00100000ul
#define IROM_OFFSET(a)(((lu_int32) (a)) - IROM0_SEG)

static void fatal(const char *message) {
  fprintf(stderr, "%s: %s\n", progname, message);
  exit(EXIT_FAILURE);
}

static void cannot(const char *what) {
  fprintf(stderr, "%s: cannot %s %s: %s\n", progname, what, output, strerror(errno));
  exit(EXIT_FAILURE);
}

static void usage(const char *message) {
  if (*message == '-')
    fprintf(stderr, "%s: unrecognized option '%s'\n", progname, message);
  else
    fprintf(stderr, "%s: %s\n", progname, message);
  fprintf(stderr,
    "usage: %s [options] [filenames]\n"
    "Available options are:\n"
    "  -l       list (use -l -l for full listing)\n"
    "  -o name  output to file 'name' (default is \"%s\")\n"
    "  -e name  execute a lua source file\n"
    "  -F name  load a flash image file\n"
    "  -a addr  generate an absolute, rather than "
    "position independent flash image file\n"
    "           (use with -F LFSimage -o absLFSimage to "
    "convert an image to absolute format)\n"
    "  -m size  maximum LFS image in bytes\n"
    "  -p       parse only\n"
    "  -s n     strip debug information\n"
    "  -v       show version information\n"
    "  --       stop handling options\n"
    "  -        stop handling options and process stdin\n", progname, Output);
  exit(EXIT_FAILURE);
}

#define IS(s)(strcmp(argv[i], s) == 0)

static int doargs(int argc, char *argv[]) {
  int i;
  int version = 0;
  lu_int32 offset = 0;
  char *p;
  if (argv[0] != NULL && *argv[0] != 0) progname = argv[0];
  for (i = 1; i < argc; i++) {
    if (*argv[i] != '-') {     /* end of options; keep it */
      break;
    } else if (IS("--")) {    /* end of options; skip it */
      ++i;
      if (version) ++version;
      break;
    } else if (IS("-")) {     /* end of options; use stdin */
      break;
    } else if (IS("-e")) {    /* execute a lua source file file */
      execute = argv[++i];
      if (execute == NULL || *execute == 0 || *execute == '-')
        usage("\"-e\" needs a file argument");
    } else if (IS("-F")) {    /* execute a lua source file file */
      LFSimageName = argv[++i];
      if (LFSimageName == NULL || *LFSimageName == 0 || *LFSimageName == '-')
        usage("\"-F\" needs an LFS image file argument");
    } else if (IS("-a")) {    /* Absolue flash image file */
//      flash = lookup = 1;
      address = strtol(argv[++i], NULL, 0);
      offset = IROM_OFFSET(address);
      if (offset == 0 || offset > IROM0_SEGMAX)
        usage("\"-a\" absolute address must be valid flash address");
    } else if (IS("-l")) {    /* list */
      ++listing;
    } else if (IS("-m")) {    /* specify a maximum image size */
//      flash = lookup = 1;
      maxSize = strtol(argv[++i], NULL, 0);
      if (maxSize &0xFFF)
        usage("\"-e\" maximum size must be a multiple of 4,096");
    } else if (IS("-o")) {    /* output file */
      output = argv[++i];
      if (output == NULL || *output == 0 || (*output == '-' && output[1] != 0))
        usage("'-o' needs argument");
      if (IS("-")) output = NULL;
    } else if (IS("-p")) {    /* parse only */
      dumping = 0;
    } else if (IS("-s")) {    /* strip debug information */
      stripping = (unsigned) strtol(argv[i+1], &p, 0);
      if (p == argv[i+1]) {
        stripping = 1;        /* -s 1 if default if -s applied */
        i++;
      }
      if (stripping > 2)
        usage("'-s' level must be 0 - 2");
    } else if (IS("-v")) {    /* show version */
      ++version;
    } else {                  /* unknown option */
      usage(argv[i]);
    }
  }

  if (version) {
    printf("%s\n", LUA_COPYRIGHT);
    if (version == argc - 1) exit(EXIT_SUCCESS);
  }
  if (offset > 0 && (output == NULL || LFSimageName == NULL ||
      execute != NULL || i != argc))
    usage("'-a' also requires '-o' and '-f' options without lua source files");

  if (i == argc && (listing || !dumping)) {
    dumping = 0;
    argv[--i] = Output;
  }
  return i;
}

static const char *corename(lua_State *L, const TString *filename, size_t *len) {
  const char *fn = getstr(filename) + 1;
  const char *s = strrchr(fn, '/');
  if (!s) s = strrchr(fn, '\\');
  s = s ? s + 1 : fn;
  while (*s == '.') s++;
  const char *e = strchr(s, '.');
  if (len)
    *len = e ? e - s : strlen(s);
  return s;
}

/*
 ** If the luac command line includes multiple files or has the -f option
 ** then luac generates a main function to reference all sub-main prototypes.
 ** This is one of two types:
 **   Type 0   The standard luac combination main
 **   Type 1   A lookup wrapper that is used for LFS image dumps
 */
#define toproto(L, i) getproto(L->top + (i))


static int writer(lua_State *L,
  const void *p, size_t size, void *u) {
  UNUSED(L);
  return (fwrite(p, size, 1, (FILE *) u) != 1) && (size != 0);
}

#include "lauxlib.h"
static int execfile(lua_State *L, const char *name);

static int pmain(lua_State *L) {
  int argc = (int) lua_tointeger(L, 1);
  const char **argv = (const char ** ) lua_touserdata(L, 2);
  int i;
  luaL_openlibs(L);
  if (!lua_checkstack(L, argc + 1)) fatal("too many input files");
  if (execute && (execfile(L, execute) != LUA_OK))
    lua_error(L);
  lua_createtable(L, argc, 0);
  for (i = 0; i < argc; i++) {
    size_t l;   
    lua_pushstring(L, IS("-") ?  "stdin" : argv[i]);
    const char *p = corename(L, tsvalue(L->top-1), &l);
    lua_pushlstring(L, p, l);
    lua_remove(L, -2);
    if (luaL_loadfile(L, IS("-") ? NULL : argv[i]) != LUA_OK)
      fatal(lua_tostring(L, -1));
    lua_rawset(L, -3);
  }
//  if (listing) luaU_print(f, listing > 1);
  (void)(PrintFunction);
  if (dumping  && argc > 0) {
    FILE *D = (output == NULL) ? stdout : fopen(output, "wb");
    if (D == NULL) cannot("open");
    lua_lock(L);
    luaU_dump(L, hvalue(L->top-1), writer, D, stripping);
    lua_unlock(L);
    if (ferror(D)) cannot("write");
    if (fclose(D)) cannot("close");
  }
  return 0;
}

int request_restart;

int main(int argc, char *argv[]) {
  lua_State *L;
  int i = doargs(argc, argv);
  int status;
  const char *err;
  argc -= i;
  argv += i;
  if (argc <= 0 && execute == 0 && address == 0) usage("no input files given");
  //if (address)
  //  luaN_setabsolute(address);

  do {
    request_restart = 0;
    L = luaL_newstate();
    if (L == NULL)
      fatal("not enough memory for state");
    lua_pushcfunction(L, &pmain);
    lua_pushinteger(L, argc);
    lua_pushlightuserdata(L, argv);
    status = lua_pcall(L, 2, 0, 0);
    err = (status != LUA_OK) ? strdup(lua_tostring(L, -1)) : NULL;
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_close(L);
  } while (request_restart);
  if (status != LUA_OK)
    fatal(err);
#if 0
  for (j = 0; j < 2; j++) {
    L = luaL_newstate();
    if (status != LUA_OK) {
      if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
        /*An LFS image has been loaded */
        if (address) {
          /* write out as absolute image and exit */
          lu_int32 size = cast(LFSHeader *, LFSregion)->flash_size;
          FILE *af = fopen(output, "wb");
          if (af == NULL) cannot("open");
          if (fwrite(LFSregion, size, 1, af) != 1) cannot("write");
          fclose(af);
          exit(0);
        }
        /*otherwise simulate a restart */
        lua_close(L);
        continue; /* and loop around once more simulating restart */
      }
      char *
      lua_close(L);
      fatal(err);
    }
    lua_close(L);
    break;
  }
#endif
  return EXIT_SUCCESS;
}
/*
**
*/

static int msghandler(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg == NULL) /* is error object not a string? */
    msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
  luaL_traceback(L, L, msg, 1); /* append a standard traceback */
  return 1; /* return the traceback */
}

static int execfile(lua_State *L, const char *name) {
  int status = luaL_loadfile(L, name);
  if (status == LUA_OK) {
    int base = lua_gettop(L);
    lua_pushcfunction(L, msghandler); /* push message handler */
    lua_insert(L, base); /* put it under function and args */
    status = lua_pcall(L, 0, 0, base);
    lua_remove(L, base); /* remove message handler from the stack */
  }
  if (status != LUA_OK) {
    const char *e = lua_tostring(L, -1);
    if (!strncmp(e,"!LFS ",5))
      lua_error(L);
    fprintf(stderr, "%s: %s\n", PROGNAME, e);
    lua_pop(L, 1); /* remove message */
  }
  return status;
}

/*
 ** $Id: luac.c,v 1.76 2018/06/19 01:32:02 lhf Exp $
 ** print bytecodes
 ** See Copyright Notice in lua.h
 */

#include <ctype.h>
#include <stdio.h>

#include "ldebug.h"
#include "lopcodes.h"

#define VOID(p)((const void *)(p))

static void PrintString(const TString *ts) {
  const char *s = getstr(ts);
  size_t i, n = tsslen(ts);
  printf("%c", '"');
  for (i = 0; i < n; i++) {
    int c = (int)(unsigned char) s[i];
    switch (c) {
    case '"':  printf("\\\""); break;
    case '\\': printf("\\\\"); break;
    case '\a': printf("\\a");  break;
    case '\b': printf("\\b");  break;
    case '\f': printf("\\f");  break;
    case '\n': printf("\\n");  break;
    case '\r': printf("\\r");  break;
    case '\t': printf("\\t");  break;
    case '\v': printf("\\v");  break;
    default:
      printf(isprint(c) ? "%c" : "\\%03d", c);
    }
  }
  printf("%c", '"');
}

static void PrintConstant(const Proto *f, int i) {
  const TValue *o = &f->k[i];
  switch (ttype(o)) {
  case LUA_TNIL:
    printf("nil");
    break;
  case LUA_TBOOLEAN:
    printf(bvalue(o) ? "true" : "false");
    break;
  case LUA_TNUMFLT:
    {
      char buff[100];
      sprintf(buff, LUA_NUMBER_FMT, fltvalue(o));
      printf("%s", buff);
      if (buff[strspn(buff, "-0123456789")] == '\0') printf(".0");
      break;
    }
  case LUA_TNUMINT:
    printf(LUA_INTEGER_FMT, ivalue(o));
    break;
  case LUA_TSHRSTR:
  case LUA_TLNGSTR:
    PrintString(tsvalue(o));
    break;
  default:
    /* cannot happen */
    printf("? type=%d", ttype(o));
    break;
  }
}

#define UPVALNAME(x)((f->upvalues[x].name) ? getstr(f->upvalues[x].name) : "-")
#define MYK(x)(-1 - (x))

static void PrintCode(const Proto *f) {
  const Instruction *code = f->code;
  int pc, n = f->sizecode;
  for (pc = 0; pc < n; pc++) {
    Instruction i = code[pc];
    OpCode o = GET_OPCODE(i);
    int a = GETARG_A(i);
    int b = GETARG_B(i);
    int c = GETARG_C(i);
    int ax = GETARG_Ax(i);
    int bx = GETARG_Bx(i);
    int sbx = GETARG_sBx(i);
    int line = getfuncline(f, pc);
    printf("\t%d\t", pc + 1);
    if (line > 0) printf("[%d]\t", line);
    else printf("[-]\t");
    printf("%-9s\t", luaP_opnames[o]);
    switch (getOpMode(o)) {
    case iABC:
      printf("%d", a);
      if (getBMode(o) != OpArgN) printf(" %d", ISK(b) ? (MYK(INDEXK(b))) : b);
      if (getCMode(o) != OpArgN) printf(" %d", ISK(c) ? (MYK(INDEXK(c))) : c);
      break;
    case iABx:
      printf("%d", a);
      if (getBMode(o) == OpArgK) printf(" %d", MYK(bx));
      if (getBMode(o) == OpArgU) printf(" %d", bx);
      break;
    case iAsBx:
      printf("%d %d", a, sbx);
      break;
    case iAx:
      printf("%d", MYK(ax));
      break;
    }
    switch (o) {
    case OP_LOADK:
      printf("\t; ");
      PrintConstant(f, bx);
      break;
    case OP_GETUPVAL:
    case OP_SETUPVAL:
      printf("\t; %s", UPVALNAME(b));
      break;
    case OP_GETTABUP:
      printf("\t; %s", UPVALNAME(b));
      if (ISK(c)) {
        printf(" ");
        PrintConstant(f, INDEXK(c));
      }
      break;
    case OP_SETTABUP:
      printf("\t; %s", UPVALNAME(a));
      if (ISK(b)) {
        printf(" ");
        PrintConstant(f, INDEXK(b));
      }
      if (ISK(c)) {
        printf(" ");
        PrintConstant(f, INDEXK(c));
      }
      break;
    case OP_GETTABLE:
    case OP_SELF:
      if (ISK(c)) {
        printf("\t; ");
        PrintConstant(f, INDEXK(c));
      }
      break;
    case OP_SETTABLE:
    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_MOD:
    case OP_POW:
    case OP_DIV:
    case OP_IDIV:
    case OP_BAND:
    case OP_BOR:
    case OP_BXOR:
    case OP_SHL:
    case OP_SHR:
    case OP_EQ:
    case OP_LT:
    case OP_LE:
      if (ISK(b) || ISK(c)) {
        printf("\t; ");
        if (ISK(b)) PrintConstant(f, INDEXK(b));
        else printf("-");
        printf(" ");
        if (ISK(c)) PrintConstant(f, INDEXK(c));
        else printf("-");
      }
      break;
    case OP_JMP:
    case OP_FORLOOP:
    case OP_FORPREP:
    case OP_TFORLOOP:
      printf("\t; to %d", sbx + pc + 2);
      break;
    case OP_CLOSURE:
      printf("\t; %p", VOID(f->p[bx]));
      break;
    case OP_SETLIST:
      if (c == 0) printf("\t; %d", (int) code[++pc]);
      else printf("\t; %d", c);
      break;
    case OP_EXTRAARG:
      printf("\t; ");
      PrintConstant(f, ax);
      break;
    default:
      break;
    }
    printf("\n");
  }
}

#define SS(x)((x == 1) ? "" : "s")
#define S(x)(int)(x), SS(x)

static void PrintHeader(const Proto *f) {
  const char *s = f->source ? getstr(f->source) : "=?";
  if (*s == '@' || *s == '=')
    s++;
  else if (*s == LUA_SIGNATURE[0])
    s = "(bstring)";
  else
    s = "(string)";
  printf("\n%s <%s:%d,%d> (%d instruction%s at %p)\n",
    (f->linedefined==0)?"main":"function", 
    s, f->linedefined, f->lastlinedefined,
    S(f->sizecode), VOID(f));
  printf("%d%s param%s, %d slot%s, %d upvalue%s, ",
    (int)(f->numparams), f->is_vararg ? "+" : "", SS(f->numparams),
    S(f->maxstacksize), S(f->sizeupvalues));
  printf("%d local%s, %d constant%s, %d function%s\n",
    S(f->sizelocvars), S(f->sizek), S(f->sizep));
}

static void PrintDebug(const Proto *f) {
  int i, n;
  n = f->sizek;
  printf("constants (%d) for %p:\n", n, VOID(f));
  for (i = 0; i < n; i++) {
    printf("\t%d\t", i + 1);
    PrintConstant(f, i);
    printf("\n");
  }
  n = f->sizelocvars;
  printf("locals (%d) for %p:\n", n, VOID(f));
  for (i = 0; i < n; i++) {
    printf("\t%d\t%s\t%d\t%d\n",
      i, getstr(f->locvars[i].varname), 
      f->locvars[i].startpc + 1, f->locvars[i].endpc + 1);
  }
  n = f->sizeupvalues;
  printf("upvalues (%d) for %p:\n", n, VOID(f));
  for (i = 0; i < n; i++) {
    printf("\t%d\t%s\t%d\t%d\n",
      i, UPVALNAME(i), f->upvalues[i].instack, f->upvalues[i].idx);
  }
}

static void PrintFunction(const Proto *f, int full) {
  int i, n = f->sizep;
  PrintHeader(f);
  PrintCode(f);
  if (full) PrintDebug(f);
  for (i = 0; i < n; i++) PrintFunction(f->p[i], full);
}
