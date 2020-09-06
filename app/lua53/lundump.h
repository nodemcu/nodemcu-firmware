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
#define LUAU_TNIL      (0<<4)
#define LUAU_TBOOLEAN  (1<<4)
#define LUAU_TNUMFLT   (2<<4)
#define LUAU_TNUMPINT  (3<<4)
#define LUAU_TNUMNINT  (4<<4)
#define LUAU_TSTRING   (5<<4)
#define LUAU_TMASK     (7<<4)
#define LUAU_DMASK     0x0f

/* data to catch conversion errors */
#define LUAC_DATA "\x19\x93\r\n\x1a\n"

#define LUAC_INT         0x5678
#define LUAC_NUM         cast_num(370.5)
#define LUAC_FUNC_MARKER 0xFE
#define LUA_TNUMNINT     (LUA_TNUMBER | (2 << 4)) /* negative integer numbers */

#define MYINT(s)  (s[0]-'0')
#define LUAC_VERSION  (MYINT(LUA_VERSION_MAJOR)*16+MYINT(LUA_VERSION_MINOR))
#define LUAC_FORMAT           10       /* this is the NodeMCU format */
#define LUAC_LFS_IMAGE_FORMAT 11
#define LUA_STRING_SIG       "\x19ss"
#define LUA_PROTO_SIG        "\x19pr"
#define LUA_HDR_BYTE         '\x19'

typedef void * (*luaU_Writer) (void *ud, const void *p, size_t sz);

typedef struct LoadState LoadState;

typedef struct luaU_data {
  stringtable  ROstrt;
  ROTable     *protoROTable;
  Proto       *protoHead;
  TString     *shortTShead;
  TString     *longTShead;
  int          tsTot;
} luaU_data;


/* load one chunk; from lundump.c */
LUAI_FUNC void luaU_undumpx (lua_State* L, ZIO* Z, const char* name, LoadState *S);
#define luaU_undump(l,z,n)  luaU_undumpx(l,z,n,NULL)
LUAI_FUNC LoadState *luaU_LoadState (lua_State *L, Table *t, int ROstrtSize,
                                     luaU_Writer fw, void *fwd,
                                     int mode, unsigned int seed);

LUAI_FUNC void luaU_closeLoadState (LoadState *S, luaU_data *res,
                                    const ROTable *indexMeta);
LUAI_FUNC void luaU_addstrings(LoadState *S, const char **s, lu_byte *e);
LUAI_FUNC int luaU_dump (lua_State *L, Table *pt, lua_Writer w,
                         void *data, int strip);
LUAI_FUNC unsigned int luaU_zlibcrc32 (const void *data, size_t length,
                                       unsigned int crc);
LUAI_FUNC const char *luaU_getbasename (lua_State *L, const TString *filename,
                                        size_t *len);
LUAI_FUNC int luaU_stripdebug (lua_State *L, Proto *f, int level, int recv);

#endif
