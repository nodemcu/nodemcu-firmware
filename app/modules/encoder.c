// Module encoders

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "c_string.h"
#define BASE64_INVALID '\xff'
#define BASE64_PADDING '='
#define ISBASE64(c) (unbytes64[c] != BASE64_INVALID)

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *toBase64 ( lua_State* L, const char *msg, size_t *len){
  size_t i, n = *len;

  if (!n)  // handle empty string case 
    return NULL;

  char* q, *out = (char*)luaM_malloc(L, (n + 2) / 3 * 4);
  char bytes64[sizeof(b64)];
  c_memcpy(bytes64, b64, sizeof(b64));   //Avoid lots of flash unaligned fetches
  
  for (i = 0, q = out; i < n; i += 3) {
    int a = msg[i];
    int b = (i + 1 < n) ? msg[i + 1] : 0;
    int c = (i + 2 < n) ? msg[i + 2] : 0;
    *q++ = bytes64[a >> 2];
    *q++ = bytes64[((a & 3) << 4) | (b >> 4)];
    *q++ = (i + 1 < n) ? bytes64[((b & 15) << 2) | (c >> 6)] : BASE64_PADDING;
    *q++ = (i + 2 < n) ? bytes64[(c & 63)] : BASE64_PADDING;
  }
  *len = q - out;
  return out;
}

static char *fromBase64 ( lua_State* L, const char *enc_msg, size_t *len){
  int i, n = *len, pad = 0;
  unsigned const char *p;
  unsigned char unbytes64[CHAR_MAX+1-CHAR_MIN], *msg, *q, a, b, c, d;

  if (!n)  // handle empty string case 
    return NULL;
 
  if (n % 4)
    luaL_error (L, "Invalid base64 string"); 
   
  c_memset(unbytes64, BASE64_INVALID, sizeof(unbytes64));
  for (i = 0; i < sizeof(b64)-1; i++) unbytes64[b64[i]] = i;  // sequential so no exceptions 
  
  if (enc_msg[n-1] == BASE64_PADDING) {
    pad =  (enc_msg[n-2] != BASE64_PADDING) ? 1 : 2;
  }    

 for (i = 0; i < n - pad; i++) if (!ISBASE64(enc_msg[i])) luaL_error (L, "Invalid base64 string");
  unbytes64[BASE64_PADDING] = 0;

  msg = q = (unsigned char*) luaM_malloc(L, 1+ (3 * n / 4)); 
  for (i = 0, p = (unsigned char*) enc_msg;; i+=4)  {
    a = unbytes64[*p++], b = unbytes64[*p++], c = unbytes64[*p++], d = unbytes64[*p++] ;
    if (i > n-4) break;
    *q++ = (a << 2) | (b >> 4);
    *q++ = (b << 4) | (c >> 2);
    *q++ = (c << 6) | d;
  }

  if (pad) {
    *q++ = (a << 2) | (b >> 4);
    if (pad == 1) *q++ = (b << 4) | (c >> 2);
  }
  *len = q - msg;
  return (char *) msg;
}

static inline char to_hex_nibble(unsigned char b) {
  return b + ( b < 10 ? '0' : 'a' - 10 );
  }
    
static char *toHex ( lua_State* L, const char *msg, size_t *len){
  int i, n = *len;
  char *q, *out = (char*)luaM_malloc(L, n * 2);
  for (i = 0, q = out; i < n; i++) {
    *q++ = to_hex_nibble(msg[i] >> 4);
    *q++ = to_hex_nibble(msg[i] & 0xf);
  }
  *len = 2*n; 
  return out;
}

static char *fromHex ( lua_State* L, const char *msg, size_t *len){
  int i, n = *len;
  const char *p;
  char b, *q, *out = (char*)luaM_malloc(L, n * 2);
  unsigned char c;
  
  if (n &1)
    luaL_error (L, "Invalid hex string");
 
  for (i = 0, p = msg, q = out; i < n; i++) {
     if (*p >= '0' && *p <= '9') {
       b = *p++ - '0';
     } else if (*p >= 'a' && *p <= 'f') {
       b = *p++ - ('a' - 10);
     } else if (*p >= 'A' && *p <= 'F') {
       b = *p++ - ('A' - 10);
     } else {
       luaL_error (L, "Invalid hex string");
     }
     if ((i&1) == 0) {
       c = b<<4;
     } else {
       *q++ = c+ b;
     }    
  }
  *len = n>>1; 
  return out;
}

// All encoder functions are of the form:
// Lua:  output_string = encoder.function(input_string) 
// Where input string maybe empty, but not nil
// Hence these all chare the do_func wrapper
static int do_func (lua_State *L, char * (*conv_func)(lua_State *, const char *, size_t *)) {
  size_t l;
  const char *input = luaL_checklstring(L, 1, &l);
//  luaL_argcheck(L, l>0, 1, "input string empty");
  char *output = conv_func(L, input, &l);
  
  if (output) {
    lua_pushlstring(L, output, l);
    luaM_free(L, output);
  } else {
    lua_pushstring(L, "");
  }
  return 1;
}

#define DECLARE_FUNCTION(f) static int encoder_ ## f (lua_State *L) \
{ return do_func(L, f); }

  DECLARE_FUNCTION(fromBase64);
  DECLARE_FUNCTION(toBase64);
  DECLARE_FUNCTION(fromHex);
  DECLARE_FUNCTION(toHex);

// Module function map
static const LUA_REG_TYPE encoder_map[] = {
  { LSTRKEY("fromBase64"), LFUNCVAL(encoder_fromBase64)  },
  { LSTRKEY("toBase64"),   LFUNCVAL(encoder_toBase64) },
  { LSTRKEY("fromHex"),    LFUNCVAL(encoder_fromHex)  },
  { LSTRKEY("toHex"),      LFUNCVAL(encoder_toHex) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(ENCODER, "encoder", encoder_map, NULL);
