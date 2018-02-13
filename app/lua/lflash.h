/*
** lflashe.h
** See Copyright Notice in lua.h
*/

#if defined(LUA_FLASH_STORE) && !defined(lflash_h)
#define lflash_h

#include "lobject.h"
#include "lstate.h"
#include "lzio.h"

#define FLASH_SIG          0xfafaaf00

typedef lu_int32 FlashAddr;
typedef struct {
  lu_int32  flash_sig;      /* a stabdard fingerprint identifying an LFS image */
  lu_int32  flash_size;     /* Size of LFS image */
  FlashAddr mainProto;      /* address of main Proto in Proto hierarchy */
  FlashAddr pROhash;        /* address of ROstrt hash */
  lu_int32  nROuse;         /* number of elements in ROstrt */
  int       nROsize;        /* size of ROstrt */
  lu_int32  fill1;          /* reserved */
  lu_int32  fill2;          /* reserved */
} FlashHeader;

void luaN_user_init(void);
LUAI_FUNC void luaN_init (lua_State *L);
LUAI_FUNC int  luaN_flashSetup (lua_State *L);
LUAI_FUNC int  luaN_reload_reboot (lua_State *L);
LUAI_FUNC int  luaN_index (lua_State *L);
#endif

