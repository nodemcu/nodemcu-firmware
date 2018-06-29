/*
** $Id: luac.c,v 1.54 2006/06/02 17:37:11 lhf Exp $
** Lua compiler (saves bytecodes to files; also list bytecodes)
** See Copyright Notice in lua.h
*/

#define LUAC_CROSS_FILE

#include "luac_cross.h"
#include C_HEADER_ERRNO
#include C_HEADER_STDIO
#include C_HEADER_STDLIB
#include C_HEADER_STRING
#include <time.h>

#define luac_c
#define LUA_CORE

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#define PROGNAME	"luac"		/* default program name */
#define	OUTPUT		PROGNAME ".out"	/* default output file */

static int listing=0;			/* list bytecodes? */
static int dumping=1;			/* dump bytecodes? */
static int stripping=0;	  /* strip debug information? */
static int flash=0;	  		/* output flash image */
static lu_int32 address=0;  /* output flash image at absolute location */
static int lookup=0;			/* output lookup-style master combination header */
static char Output[]={ OUTPUT };	/* default output file name */
static const char* output=Output;	/* actual output file name */
static const char* execute;       /* executed a Lua file */
static const char* progname=PROGNAME;	/* actual program name */
static DumpTargetInfo target;

void luac_fatal(const char* message)
{
 fprintf(stderr,"%s: %s\n",progname,message);
 exit(EXIT_FAILURE);
}
#define fatal(s) luac_fatal(s)


static void cannot(const char* what)
{
 fprintf(stderr,"%s: cannot %s %s: %s\n",progname,what,output,strerror(errno));
 exit(EXIT_FAILURE);
}

static void usage(const char* message)
{
 if (*message=='-')
  fprintf(stderr,"%s: unrecognized option " LUA_QS "\n",progname,message);
 else
  fprintf(stderr,"%s: %s\n",progname,message);
 fprintf(stderr,
 "usage: %s [options] [filenames].\n"
 "Available options are:\n"
 "  -        process stdin\n"
 "  -l       list\n"
 "  -o name  output to file " LUA_QL("name") " (default is \"%s\")\n"
 "  -e name  execute a lua source file\n"
 "  -f       output a flash image file\n"
 "  -a addr  generate an absolute, rather than position independent flash image file\n"
 "  -i       generate lookup combination master (default with option -f)\n" 
 "  -p       parse only\n"
 "  -s       strip debug information\n"
 "  -v       show version information\n"
 "  --       stop handling options\n",
 progname,Output);
 exit(EXIT_FAILURE);
}

#define	IS(s)	(strcmp(argv[i],s)==0)
#define IROM0_SEG    0x40210000ul
#define IROM0_SEGMAX 0x00100000ul

static int doargs(int argc, char* argv[])
{
 int i;
 int version=0;
 if (argv[0]!=NULL && *argv[0]!=0) progname=argv[0];
 for (i=1; i<argc; i++)
 {
  if (*argv[i]!='-')			/* end of options; keep it */
   break;
  else if (IS("--"))			/* end of options; skip it */
  {
   ++i;
   if (version) ++version;
   break;
  }
  else if (IS("-"))		  	/* end of options; use stdin */
   break;
  else if (IS("-e"))			/* execute a lua source file file */
  {
   execute=argv[++i];
   if (execute ==NULL || *execute==0 || *execute=='-' ) 
     usage(LUA_QL("-e") " needs argument");
  }
  else if (IS("-f"))			/* Flash image file */
  {
   flash=lookup=1;
  }
  else if (IS("-a"))			/* Absolue flash image file */
  {
   flash=lookup=1;
   address=strtol(argv[++i],NULL,0);
   size_t offset = (unsigned) (address -IROM0_SEG);
   if (offset > IROM0_SEGMAX)
     usage(LUA_QL("-e") " absolute address must be valid flash address");
  }
  else if (IS("-i"))			/* lookup */
   lookup = 1;
  else if (IS("-l"))			/* list */
   ++listing;
  else if (IS("-o"))			/* output file */
  {
   output=argv[++i];
   if (output==NULL || *output==0) usage(LUA_QL("-o") " needs argument");
   if (IS("-")) output=NULL;
  }

  else if (IS("-p"))			/* parse only */
   dumping=0;   
  else if (IS("-s"))			/* strip debug information */
   stripping=1;
  else if (IS("-v"))			/* show version */
   ++version;
  else					/* unknown option */
  usage(argv[i]);
 }

 if (i==argc && (listing || !dumping))
 {
  dumping=0;
  argv[--i]=Output;
 }
 if (version)
 {
  printf("%s  %s\n",LUA_RELEASE,LUA_COPYRIGHT);
  if (version==argc-1) exit(EXIT_SUCCESS);
 }
 return i;
}

#define toproto(L,i) (clvalue(L->top+(i))->l.p)

static TString *corename(lua_State *L, const TString *filename) 
{
 const char *fn = getstr(filename)+1;
 const char *s = strrchr(fn, '/');
 s = s ? s + 1 : fn;
 while (*s == '.') s++;
 const char *e = strchr(s, '.');
 int l = e ? e - s: strlen(s);
 return l ? luaS_newlstr (L, s, l) : luaS_new(L, fn);
} 
/*
 * If the luac command line includes multiple files or has the -f option 
 * then luac generates a main function to reference all sub-main prototypes.
 * This is one of two types:
 *   Type 0   The standard luac combination main
 *   Type 1   A lookup wrapper that facilitates indexing into the generated protos 
 */
static const Proto* combine(lua_State* L, int n, int type)
{
 if (n==1 && type == 0)
  return toproto(L,-1);
 else
 {
  int i;
  Instruction *pc;
  Proto* f=luaF_newproto(L);
  setptvalue2s(L,L->top,f); incr_top(L);
  f->source=luaS_newliteral(L,"=(" PROGNAME ")");
  f->p=luaM_newvector(L,n,Proto*);
  f->sizep=n;
  for (i=0; i<n; i++) 
    f->p[i]=toproto(L,i-n-1);
  pc=0;

  if (type == 0) {
  /*
   * Type 0 is as per the standard luac, which is just a main routine which 
   * invokes all of the compiled functions sequentially.  This is fine if 
   * they are self registering modules, but useless otherwise.
   */
   f->numparams    = 0;
   f->maxstacksize = 1;
   f->sizecode     = 2*n + 1 ;
   f->sizek        = 0;
   f->code         = luaM_newvector(L, f->sizecode , Instruction);
   f->k            = luaM_newvector(L,f->sizek,TValue);

   for (i=0, pc = f->code; i<n; i++) {
    *pc++ = CREATE_ABx(OP_CLOSURE,0,i);
    *pc++ = CREATE_ABC(OP_CALL,0,1,1);
   }
   *pc++ = CREATE_ABC(OP_RETURN,0,1,0);
  } else {
  /*
   * The Type 1 main() is a lookup which takes a single argument, the name to  
   * be resolved. If this matches root name of one of the compiled files then
   * a closure to this file main is returned.  Otherwise the Unixtime of the
   * compile and the list of root names is returned.
   */
   if (n > LFIELDS_PER_FLUSH) {
#define NO_MOD_ERR_(n) ": Number of modules > " #n
#define NO_MOD_ERR(n) NO_MOD_ERR_(n)
    usage(LUA_QL("-f")  NO_MOD_ERR(LFIELDS_PER_FLUSH));
   }
   f->numparams    = 1;
   f->maxstacksize = n + 3;
   f->sizecode     = 5*n + 5 ;
   f->sizek        = n + 1;
   f->sizelocvars  = 0;
   f->code         = luaM_newvector(L, f->sizecode , Instruction);
   f->k            = luaM_newvector(L,f->sizek,TValue);
   for (i=0, pc = f->code; i<n; i++)  
   {
    /* if arg1 == FnameA then return function (...) -- funcA -- end end */
    setsvalue2n(L,f->k+i,corename(L, f->p[i]->source));
    *pc++ = CREATE_ABC(OP_EQ,0,0,RKASK(i)); 
    *pc++ = CREATE_ABx(OP_JMP,0,MAXARG_sBx+2);
    *pc++ = CREATE_ABx(OP_CLOSURE,1,i);
    *pc++ = CREATE_ABC(OP_RETURN,1,2,0);
   }

   setnvalue(f->k+n, (lua_Number) time(NULL));

   *pc++ = CREATE_ABx(OP_LOADK,1,n);
   *pc++ = CREATE_ABC(OP_NEWTABLE,2,luaO_int2fb(i),0);   
   for (i=0; i<n; i++) 
     *pc++ = CREATE_ABx(OP_LOADK,i+3,i);
   *pc++ = CREATE_ABC(OP_SETLIST,2,i,1);   
   *pc++ = CREATE_ABC(OP_RETURN,1,3,0);
   *pc++ = CREATE_ABC(OP_RETURN,0,1,0);
  }
  lua_assert((pc-f->code) == f->sizecode);

  return f;
 }
}

static int writer(lua_State* L, const void* p, size_t size, void* u)
{
 UNUSED(L);
 return (fwrite(p,size,1,(FILE*)u)!=1) && (size!=0);
}

struct Smain {
 int argc;
 char** argv;
};

extern uint dumpToFlashImage (lua_State* L,const Proto *main, lua_Writer w, 
                              void* data, int strip, lu_int32 address);

static int pmain(lua_State* L)
{
 struct Smain* s = (struct Smain*)lua_touserdata(L, 1);
 int argc=s->argc;
 char** argv=s->argv;
 const Proto* f;
 int i;
 if (!lua_checkstack(L,argc)) fatal("too many input files");
 if (execute)
 {
  if (luaL_loadfile(L,execute)!=0) fatal(lua_tostring(L,-1));
  luaL_openlibs(L);
  lua_pushstring(L, execute);
  if (lua_pcall(L, 1, 1, 0)) fatal(lua_tostring(L,-1));
  if (!lua_isfunction(L, -1)) 
  {
   lua_pop(L,1);
   if(argc == 0) return 0;
   execute = NULL;
  }
 }
 for (i=0; i<argc; i++)
 {
  const char* filename=IS("-") ? NULL : argv[i];
  if (luaL_loadfile(L,filename)!=0) fatal(lua_tostring(L,-1));
 }
 f=combine(L,argc + (execute ? 1: 0), lookup);
 if (listing) luaU_print(f,listing>1);
 if (dumping)
 {
  int result;
  FILE* D= (output==NULL) ? stdout : fopen(output,"wb");
  if (D==NULL) cannot("open");
  lua_lock(L);
  if (flash) 
  {
    result=dumpToFlashImage(L,f,writer, D, stripping, address);
  } else
  {
    result=luaU_dump_crosscompile(L,f,writer,D,stripping,target);
  }  
  lua_unlock(L);
  if (result==LUA_ERR_CC_INTOVERFLOW) fatal("value too big or small for target integer type");
  if (result==LUA_ERR_CC_NOTINTEGER) fatal("target lua_Number is integral but fractional value found");
  if (ferror(D)) cannot("write");
  if (fclose(D)) cannot("close");
 }
 return 0;
}

int main(int argc, char* argv[])
{
 lua_State* L;
 struct Smain s;
 
 int test=1;
 target.little_endian=*(char*)&test;
 target.sizeof_int=sizeof(int);
 target.sizeof_strsize_t=sizeof(strsize_t);
 target.sizeof_lua_Number=sizeof(lua_Number);
 target.lua_Number_integral=(((lua_Number)0.5)==0);
 target.is_arm_fpa=0;

 int i=doargs(argc,argv);
 argc-=i; argv+=i;
 if (argc<=0 && execute==0) usage("no input files given");
 L=lua_open();
 if (L==NULL) fatal("not enough memory for state");
 s.argc=argc;
 s.argv=argv;
 if (lua_cpcall(L,pmain,&s)!=0) fatal(lua_tostring(L,-1));
 lua_close(L);
 return EXIT_SUCCESS;
}
