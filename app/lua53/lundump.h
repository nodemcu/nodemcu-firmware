/*
** $Id: lundump.h,v 1.45.1.1 2017/04/19 17:20:42 roberto Exp $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"

/* These allow a multi=byte flag:1,type:3,data:4 packing in the tt byte */
#define LUAU_TNIL		    (0<<4)
#define LUAU_TBOOLEAN		(1<<4)
#define LUAU_TNUMFLT		(2<<4)
#define LUAU_TNUMPINT		(3<<4)
#define LUAU_TNUMNINT		(4<<4)
#define LUAU_TSSTRING		(5<<4)
#define LUAU_TLSTRING		(6<<4)
#define LUAU_TMASK      (7<<4)
#define LUAU_DMASK      0x0f


/* data to catch conversion errors */
#define LUAC_DATA	"\x19\x93\r\n\x1a\n"

#define LUAC_INT	0x5678
#define LUAC_NUM	cast_num(370.5)
#define LUAC_FUNC_MARKER 0xFE
#define LUA_TNUMNINT  	(LUA_TNUMBER | (2 << 4))  /* negative integer numbers */

#define MYINT(s)	(s[0]-'0')
#define LUAC_VERSION	(MYINT(LUA_VERSION_MAJOR)*16+MYINT(LUA_VERSION_MINOR))
#define LUAC_FORMAT         	10	     /* this is the NodeMCU format */
#define LUAC_LFS_IMAGE_FORMAT 11
#define LUA_STRING_SIG       "\x19ss"
#define LUA_PROTO_SIG        "\x19pr"
#define LUA_HDR_BYTE         '\x19'
/* load one chunk; from lundump.c */
LUAI_FUNC LClosure* luaU_undump (lua_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
LUAI_FUNC int luaU_dump (lua_State* L, const Proto* f, lua_Writer w,
                         void* data, int strip);
LUAI_FUNC int luaU_DumpAllProtos(lua_State *L, const Proto *m, lua_Writer w,
                         void *data, int strip);

LUAI_FUNC int luaU_undumpLFS(lua_State *L, ZIO *Z, int isabs);
#endif
