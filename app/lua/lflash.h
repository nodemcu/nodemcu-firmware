/*
** lflashe.h
** See Copyright Notice in lua.h
*/

#if defined(LUA_FLASH_STORE) && !defined(lflash_h)
#define lflash_h

#include "lobject.h"
#include "lstate.h"
#include "lzio.h"

#ifdef LUA_NUMBER_INTEGRAL
# define FLASH_SIG_B1 0x02
#else
# define FLASH_SIG_B1 0x00
#endif

#ifdef LUA_PACK_TVALUES
#ifdef LUA_NUMBER_INTEGRAL
#error "LUA_PACK_TVALUES is only valid for Floating point builds" 
#endif
# define FLASH_SIG_B2 0x04
#else
# define FLASH_SIG_B2 0x00
#endif
#define FLASH_SIG_ABSOLUTE    0x01
#define FLASH_SIG_IN_PROGRESS 0x08
#define FLASH_SIG  (0xfafaaf50 | FLASH_SIG_B2 | FLASH_SIG_B1)

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

LUAI_FUNC void luaN_init (lua_State *L);
LUAI_FUNC int  luaN_flashSetup (lua_State *L);
LUAI_FUNC int  luaN_reload_reboot (lua_State *L);
LUAI_FUNC int  luaN_index (lua_State *L);
#endif

