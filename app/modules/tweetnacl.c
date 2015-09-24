// Module providing tweetnacl api

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"
#include "user_interface.h"

#include "../tweetnacl/tweetnacl.h"
#include "c_stdlib.h"

// from tweetnacl.c
typedef unsigned char u8;

// this is provided by libwpa.a (i think)
// but nodemcu does not have the proper headers (orig in sdk:include/osapi.h)
// so manually inserting the function extern signature here
typedef unsigned int size_t;
extern int os_get_random(unsigned char *buf, size_t len);

void randombytes(u8 *buf,u64 len) { os_get_random((unsigned char *)buf, (size_t) len); };

// r = randombytes(len);
static int tweetnacl_randombytes( lua_State* L )
{
  unsigned int len;
  len = luaL_checkinteger( L, 1 );
  u8 *rbytes = (u8 *)c_malloc(len);
  randombytes(rbytes, len);
  lua_pushlstring(L, rbytes, len);
  return 1;
}

// n = nonce();
static int tweetnacl_nonce( lua_State* L )
{
  unsigned int len = crypto_box_NONCEBYTES;
  u8 *rbytes = (u8 *)c_malloc(len);
  randombytes(rbytes, len);
  lua_pushlstring(L, rbytes, len);
  c_free(rbytes);
  return 1;
}

// pk,sk = crypto_box_keypair()
static int tweetnacl_crypto_box_keypair( lua_State* L )
{
  u8 secret_key[crypto_box_SECRETKEYBYTES] = {0};
  u8 public_key[crypto_box_PUBLICKEYBYTES] = {0};

  crypto_box_keypair(public_key, secret_key);
  lua_pushlstring(L, public_key, sizeof(public_key));
  lua_pushlstring(L, secret_key, sizeof(secret_key));
  return 2;
}

// encrypted_bytes = crypto_box(clear_bytes, nonce, pk_other, sk_self);
static int tweetnacl_crypto_box( lua_State* L )
{
  unsigned int msg_len;
  const char* msg = luaL_checklstring(L,1,&msg_len);
  if(msg_len < 1)
    return luaL_error( L, "len(message)=%d, too short", msg_len);

  unsigned int nonce_len;
  const char* nonce = luaL_checklstring(L,2,&nonce_len);
  if(nonce_len != crypto_box_NONCEBYTES)
    return luaL_error( L, "len(nonce)=%d, should be %d", nonce_len, crypto_box_NONCEBYTES);

  unsigned int pk_len;
  const char* pk_other = luaL_checklstring(L,3,&pk_len);
  if(pk_len != crypto_box_PUBLICKEYBYTES)
    return luaL_error( L, "len(pk)=%d, should be %d", pk_len, crypto_box_PUBLICKEYBYTES);

  unsigned int sk_len;
  const char* sk_self = luaL_checklstring(L,4,&sk_len);
  if(sk_len != crypto_box_SECRETKEYBYTES)
    return luaL_error( L, "len(sk)=%d, should be %d", sk_len, crypto_box_SECRETKEYBYTES);

  unsigned int cmsg_len = msg_len + crypto_box_ZEROBYTES;
  char *cmsg = (char *)c_malloc(cmsg_len);
  if(!cmsg)
    return luaL_error( L, "malloc failed, %d bytes", cmsg_len);

  int i;
  for(i=0;i<crypto_box_ZEROBYTES;i++) cmsg[i] = 0;
  for(;i<cmsg_len;i++) cmsg[i] = msg[i-crypto_box_ZEROBYTES];

  unsigned int emsg_len = cmsg_len;
  char *emsg = (char *)c_malloc(emsg_len);
  if(!emsg)
  {
    c_free(cmsg);
    return luaL_error( L, "malloc failed, %d bytes", emsg_len);
  }

  crypto_box(emsg, cmsg, cmsg_len, nonce, pk_other, sk_self);
  lua_pushlstring(L, emsg+crypto_box_BOXZEROBYTES, emsg_len-crypto_box_BOXZEROBYTES);
  c_free(cmsg);
  c_free(emsg);
  return 1;
}

// shared_secret = crypto_box_beforenm(pk_other, sk_self);
static int tweetnacl_crypto_box_beforenm( lua_State* L )
{
  unsigned int pk_len;
  const char* pk_other = luaL_checklstring(L,1,&pk_len);
  if(pk_len != crypto_box_PUBLICKEYBYTES)
    return luaL_error( L, "len(pk)=%d, should be %d", pk_len, crypto_box_PUBLICKEYBYTES);

  unsigned int sk_len;
  const char* sk_self = luaL_checklstring(L,2,&sk_len);
  if(sk_len != crypto_box_SECRETKEYBYTES)
    return luaL_error( L, "len(sk)=%d, should be %d", sk_len, crypto_box_SECRETKEYBYTES);

  char secret[crypto_box_BEFORENMBYTES];

  crypto_box_beforenm(secret, pk_other, sk_self);
  lua_pushlstring(L, secret, crypto_box_BEFORENMBYTES);

  return 1;
}

// enc_msg = crypto_box_afternm(msg, nonce, shared_secret);
static int tweetnacl_crypto_box_afternm( lua_State* L )
{
  unsigned int msg_len;
  const char* msg = luaL_checklstring(L,1,&msg_len);
  if(msg_len < 1)
    return luaL_error( L, "len(message)=%d, too short", msg_len);

  unsigned int nonce_len;
  const char* nonce = luaL_checklstring(L,2,&nonce_len);
  if(nonce_len != crypto_box_NONCEBYTES)
    return luaL_error( L, "len(nonce)=%d, should be %d", nonce_len, crypto_box_NONCEBYTES);

  unsigned int secret_len;
  const char* secret = luaL_checklstring(L,3,&secret_len);
  if(secret_len != crypto_box_BEFORENMBYTES)
    return luaL_error( L, "len(secret)=%d, should be %d", secret_len, crypto_box_BEFORENMBYTES);

  unsigned int cmsg_len = msg_len + crypto_box_ZEROBYTES;
  char *cmsg = (char *)c_malloc(cmsg_len);
  if(!cmsg)
    return luaL_error( L, "malloc failed, %d bytes", cmsg_len);

  int i;
  for(i=0;i<crypto_box_ZEROBYTES;i++) cmsg[i] = 0;
  for(;i<cmsg_len;i++) cmsg[i] = msg[i-crypto_box_ZEROBYTES];

  unsigned int emsg_len = cmsg_len;
  char *emsg = (char *)c_malloc(emsg_len);
  if(!emsg)
  {
    c_free(cmsg);
    return luaL_error( L, "malloc failed, %d bytes", emsg_len);
  }

  crypto_box_afternm(emsg, cmsg, cmsg_len, nonce, secret);
  lua_pushlstring(L, emsg+crypto_box_BOXZEROBYTES, emsg_len-crypto_box_BOXZEROBYTES);
  c_free(cmsg);
  c_free(emsg);
  return 1;
}

// decrypted_bytes = crypto_box_open(enc_bytes, nonce, pk_other, sk_self);
static int tweetnacl_crypto_box_open( lua_State* L )
{
  unsigned int emsg_len;
  const char* emsg = luaL_checklstring(L,1,&emsg_len);
  if(emsg_len < crypto_box_BOXZEROBYTES)
    return luaL_error( L, "len(emessage)=%d, too short", emsg_len);

  unsigned int nonce_len;
  const char* nonce = luaL_checklstring(L,2,&nonce_len);
  if(nonce_len != crypto_box_NONCEBYTES)
    return luaL_error( L, "len(nonce)=%d, should be %d", nonce_len, crypto_box_NONCEBYTES);

  unsigned int pk_len;
  const char* pk_other = luaL_checklstring(L,3,&pk_len);
  if(pk_len != crypto_box_PUBLICKEYBYTES)
    return luaL_error( L, "len(pk)=%d, should be %d", pk_len, crypto_box_PUBLICKEYBYTES);

  unsigned int sk_len;
  const char* sk_self = luaL_checklstring(L,4,&sk_len);
  if(sk_len != crypto_box_SECRETKEYBYTES)
    return luaL_error( L, "len(sk)=%d, should be %d", sk_len, crypto_box_SECRETKEYBYTES);

  unsigned int pmsg_len = emsg_len + crypto_box_BOXZEROBYTES;
  char *pmsg = (char *)c_malloc(pmsg_len);
  if(!pmsg)
    return luaL_error( L, "malloc failed, %d bytes", pmsg_len);

  int i;
  for(i=0;i<crypto_box_BOXZEROBYTES;i++) pmsg[i] = 0;
  for(;i<pmsg_len;i++) pmsg[i] = emsg[i-crypto_box_BOXZEROBYTES];

  unsigned int cmsg_len = pmsg_len;
  char *cmsg = (char *)c_malloc(cmsg_len);
  if(!cmsg)
  {
    c_free(pmsg);
    return luaL_error( L, "malloc failed, %d bytes", cmsg_len);
  }

  int r;
  r = crypto_box_open(cmsg, pmsg, pmsg_len, nonce, pk_other, sk_self);
  if(r!=0)
  {
    c_free(cmsg);
    c_free(pmsg);
    return luaL_error( L, "decryption failed");
  } 
  lua_pushlstring(L, cmsg+crypto_box_ZEROBYTES, cmsg_len-crypto_box_ZEROBYTES);
  c_free(cmsg);
  c_free(pmsg);
  return 1;
}

// decrypted_bytes = crypto_box_open_afternm(enc_bytes, nonce, shared_secret);
static int tweetnacl_crypto_box_open_afternm( lua_State* L )
{
  unsigned int emsg_len;
  const char* emsg = luaL_checklstring(L,1,&emsg_len);
  if(emsg_len < crypto_box_BOXZEROBYTES)
    return luaL_error( L, "len(emessage)=%d, too short", emsg_len);

  unsigned int nonce_len;
  const char* nonce = luaL_checklstring(L,2,&nonce_len);
  if(nonce_len != crypto_box_NONCEBYTES)
    return luaL_error( L, "len(nonce)=%d, should be %d", nonce_len, crypto_box_NONCEBYTES);

  unsigned int secret_len;
  const char* secret = luaL_checklstring(L,3,&secret_len);
  if(secret_len != crypto_box_BEFORENMBYTES)
    return luaL_error( L, "len(secret)=%d, should be %d", secret_len, crypto_box_BEFORENMBYTES);

  unsigned int pmsg_len = emsg_len + crypto_box_BOXZEROBYTES;
  char *pmsg = (char *)c_malloc(pmsg_len);
  if(!pmsg)
    return luaL_error( L, "malloc failed, %d bytes", pmsg_len);

  int i;
  for(i=0;i<crypto_box_BOXZEROBYTES;i++) pmsg[i] = 0;
  for(;i<pmsg_len;i++) pmsg[i] = emsg[i-crypto_box_BOXZEROBYTES];

  unsigned int cmsg_len = pmsg_len;
  char *cmsg = (char *)c_malloc(cmsg_len);
  if(!cmsg)
  {
    c_free(pmsg);
    return luaL_error( L, "malloc failed, %d bytes", cmsg_len);
  }

  int r;
  r = crypto_box_open_afternm(cmsg, pmsg, pmsg_len, nonce, secret);
  if(r!=0)
  {
    c_free(cmsg);
    c_free(pmsg);
    return luaL_error( L, "decryption failed");
  } 

  lua_pushlstring(L, cmsg+crypto_box_ZEROBYTES, cmsg_len-crypto_box_ZEROBYTES);
  c_free(cmsg);
  c_free(pmsg);
  return 1;
}


// enc_msg = crypto_secretbox(msg, nonce, shared_secret);
static int tweetnacl_crypto_secretbox( lua_State* L )
{
  unsigned int msg_len;
  const char* msg = luaL_checklstring(L,1,&msg_len);
  if(msg_len < 1)
    return luaL_error( L, "len(message)=%d, too short", msg_len);

  unsigned int nonce_len;
  const char* nonce = luaL_checklstring(L,2,&nonce_len);
  if(nonce_len != crypto_box_NONCEBYTES)
    return luaL_error( L, "len(nonce)=%d, should be %d", nonce_len, crypto_box_NONCEBYTES);

  unsigned int secret_len;
  const char* secret = luaL_checklstring(L,3,&secret_len);
  if(secret_len != crypto_box_BEFORENMBYTES)
    return luaL_error( L, "len(secret)=%d, should be %d", secret_len, crypto_box_BEFORENMBYTES);

  unsigned int cmsg_len = msg_len + crypto_box_ZEROBYTES;
  char *cmsg = (char *)c_malloc(cmsg_len);
  if(!cmsg)
    return luaL_error( L, "malloc failed, %d bytes", cmsg_len);

  int i;
  for(i=0;i<crypto_box_ZEROBYTES;i++) cmsg[i] = 0;
  for(;i<cmsg_len;i++) cmsg[i] = msg[i-crypto_box_ZEROBYTES];

  unsigned int emsg_len = cmsg_len;
  char *emsg = (char *)c_malloc(emsg_len);
  if(!emsg)
  {
    c_free(cmsg);
    return luaL_error( L, "malloc failed, %d bytes", emsg_len);
  }

  crypto_box_afternm(emsg, cmsg, cmsg_len, nonce, secret);
  lua_pushlstring(L, emsg+crypto_box_BOXZEROBYTES, emsg_len-crypto_box_BOXZEROBYTES);
  c_free(cmsg);
  c_free(emsg);
  return 1;
}


// decrypted_bytes = crypto_secretbox_open(enc_bytes, nonce, shared_secret);
static int tweetnacl_crypto_secretbox_open( lua_State* L )
{
  unsigned int emsg_len;
  const char* emsg = luaL_checklstring(L,1,&emsg_len);
  if(emsg_len < crypto_box_BOXZEROBYTES)
    return luaL_error( L, "len(emessage)=%d, too short", emsg_len);

  unsigned int nonce_len;
  const char* nonce = luaL_checklstring(L,2,&nonce_len);
  if(nonce_len != crypto_box_NONCEBYTES)
    return luaL_error( L, "len(nonce)=%d, should be %d", nonce_len, crypto_box_NONCEBYTES);

  unsigned int secret_len;
  const char* secret = luaL_checklstring(L,3,&secret_len);
  if(secret_len != crypto_box_BEFORENMBYTES)
    return luaL_error( L, "len(secret)=%d, should be %d", secret_len, crypto_box_BEFORENMBYTES);

  unsigned int pmsg_len = emsg_len + crypto_box_BOXZEROBYTES;
  char *pmsg = (char *)c_malloc(pmsg_len);
  if(!pmsg)
    return luaL_error( L, "malloc failed, %d bytes", pmsg_len);

  int i;
  for(i=0;i<crypto_box_BOXZEROBYTES;i++) pmsg[i] = 0;
  for(;i<pmsg_len;i++) pmsg[i] = emsg[i-crypto_box_BOXZEROBYTES];

  unsigned int cmsg_len = pmsg_len;
  char *cmsg = (char *)c_malloc(cmsg_len);
  if(!cmsg)
  {
    c_free(pmsg);
    return luaL_error( L, "malloc failed, %d bytes", cmsg_len);
  }

  int r;
  r = crypto_box_open_afternm(cmsg, pmsg, pmsg_len, nonce, secret);
  if(r!=0)
  {
    c_free(cmsg);
    c_free(pmsg);
    return luaL_error( L, "decryption failed");
  } 

  lua_pushlstring(L, cmsg+crypto_box_ZEROBYTES, cmsg_len-crypto_box_ZEROBYTES);
  c_free(cmsg);
  c_free(pmsg);
  return 1;
}

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE tweetnacl_map[] = 
{
  { LSTRKEY( "randombytes" ), LFUNCVAL( tweetnacl_randombytes ) },
  { LSTRKEY( "nonce" ), LFUNCVAL( tweetnacl_nonce ) },
  { LSTRKEY( "crypto_box_keypair" ), LFUNCVAL( tweetnacl_crypto_box_keypair ) },
  { LSTRKEY( "crypto_box" ), LFUNCVAL( tweetnacl_crypto_box ) },
  { LSTRKEY( "crypto_box_beforenm" ), LFUNCVAL( tweetnacl_crypto_box_beforenm ) },
  { LSTRKEY( "crypto_box_afternm" ), LFUNCVAL( tweetnacl_crypto_box_afternm ) },
  { LSTRKEY( "crypto_box_open" ), LFUNCVAL( tweetnacl_crypto_box_open ) },
  { LSTRKEY( "crypto_box_open_afternm" ), LFUNCVAL( tweetnacl_crypto_box_open_afternm ) },
  { LSTRKEY( "crypto_secretbox" ), LFUNCVAL( tweetnacl_crypto_secretbox ) },
  { LSTRKEY( "crypto_secretbox_open" ), LFUNCVAL( tweetnacl_crypto_secretbox_open ) },
#if LUA_OPTIMIZE_MEMORY > 0

#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_tweetnacl( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_TWEETNACL, tweetnacl_map );
  // Add constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
