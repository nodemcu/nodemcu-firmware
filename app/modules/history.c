/*
 * app/modules/history.c
 *
 * See Copyright Notice in lua.h
 */
#include "lua.h"
#include "lmem.h"
#include "lauxlib.h"
#include "module.h"
#include "user_modules.h"
#include "c_string.h"

#define buffer_HEADER_SIZE   sizeof(buffer);
#define START_REC             "\x0d*** Start ***"
#define START_REC_LEN         (sizeof(START_REC)-1)
#define BUFFER_METATABLE     "history.buffer"

#define RETRIEVE_HEADER(L) \
  buffer *hdr = (buffer *)luaL_checkudata(L, 1, BUFFER_METATABLE)
#define RETRIEVE_BUFFER(L) \
  RETRIEVE_HEADER(L); byte   *buf = hdr->header + (sizeof(hdr->header))
  
#define WRAP1(fld) do { if (fld == hdr->size) fld = 0; } while (0)
#define WRAP(fld)  do { if (fld >= hdr->size) fld -= hdr->size; } while (0)

#define INVALID               ((uint16) -1)

#ifdef DEBUG_HISTORY
#define DUMP_HEADER(h,s) c_printf("hdr:%s  %6u  %6u  %6u  %6u  %6u  %6u\n", \
        #s, h->size, h->head, h->tail, h->free, h->reader, h->nrecs)
#else
#define DUMP_HEADER(h,s)
#endif

typedef unsigned char byte;
typedef unsigned short int uint16;

typedef union {
  struct {
	  uint16 size, head, tail, free, reader, nrecs;
	};
	struct {
	  byte header[6		 * sizeof(uint16)];
	};
} buffer;

// Move the tail or reader offset onto the next record
static uint16 bump(buffer *hdr, uint16 *hdr_field) {
  byte  *buf = hdr->header + (sizeof(hdr->header));
  
  uint16  len = buf[(*hdr_field)++];
  WRAP1(*hdr_field);
  
  if (len & 0x80) {
    len += (buf[(*hdr_field)++]<<7) - 0x80;
    WRAP1(*hdr_field);
  }

  *hdr_field += len;
  WRAP(*hdr_field);
  
  return len;
}

// Add a records to the buffer with to break and wrap at the buffer top
static void push(buffer *hdr, const   byte *rec, uint16 len) {
  byte   *buf = hdr->header + (sizeof(hdr->header));
  uint16 split = 0;

  if ((hdr->head + len) > hdr->size) {
    split = hdr->size - hdr->head;
    memcpy(buf + hdr->head, rec, split);
    rec += split;
    hdr->head = 0;
  }

  memcpy(buf + hdr->head, rec, len - split);
  hdr->head += len - split;
  hdr->free -= len;
}

// Lua: handle = history.buffer(size)
// Create a new cylic history buffer
static int history_buffer( lua_State* L ) {
  int size = luaL_checkinteger(L, 1);
  luaL_argcheck(L, size >= 1024 && size <=32768, 1, "buffer size is 1-32Kb");
  buffer *hdr = (buffer *)lua_newuserdata(L, size);
  byte *buf = hdr->header + (sizeof(hdr->header));

  size -= sizeof(hdr->header);
  memset(buf, 0, size);

  strcpy((char *)buf, START_REC);
  hdr->size   = size;
  hdr->head   = START_REC_LEN;
  hdr->free   = size - START_REC_LEN;
  hdr->tail   = 0;
  hdr->nrecs  = 1;
  hdr->reader = INVALID;

  luaL_getmetatable(L, BUFFER_METATABLE);
  lua_setmetatable(L, -2);
  DUMP_HEADER(hdr,new);
  return 1;
}

// Lua: buffer:add(data)
// Lua: buffer:add(forrmat, ...)
static int history_add( lua_State* L ) {
  RETRIEVE_HEADER(L);
  size_t len = 0, prefix_len = 1;
  byte prefix_bytes[2];
  lua_remove(L, 1);
  int nargs = lua_gettop(L);

  if (nargs>1) { // Treat the armument list as a string.format(
    lua_getfield(L, LUA_GLOBALSINDEX, "string");
    lua_getfield(L, -1, "format");
    lua_insert(L, 1); //move string.format function below arguments and dump top copy
    lua_pop(L, 1);      
    lua_call(L, nargs,1) ;     // call encoder.xxx(string)
  }

  const byte *s = (const byte *) luaL_checklstring (L, 1, &len);
  luaL_argcheck(L, len<(hdr->size-2), 1, "record longer than buffer");

  // compute the 1-2 byte length prefix. Records are not nullchar terminated.
  prefix_bytes[0] = len;
  if (len >= 128) {
    prefix_bytes[0] |= 0x80;
    prefix_bytes[1]  = len >>7;
    prefix_len = 2;
  }

  // dump enough tail records to make room for this new record
  int want = len + prefix_len - hdr->free;
  while (want > 0) {
    if (hdr->tail == hdr->reader) hdr->reader = INVALID;
    uint16 tail_len = bump(hdr, &hdr->tail);
    tail_len += len<0x80 ? 1 : 2;
    want -= tail_len;
    hdr->free += tail_len;
    hdr->nrecs--;
  }

  // now add the new record
  push(hdr, prefix_bytes, prefix_len);
  push(hdr, s, len);
  hdr->nrecs++;
  DUMP_HEADER(hdr,add);
  return 0;
}

// Lua: found=buffer:find(n)
// Returns the number of readable records from an offest.
// n >=0 w.r.t the oldest log record
// n < 0 w.r.t the youngest log record
// If the number is > 0 then the reader is positioned at this record.
static int history_find( lua_State* L ) {
  RETRIEVE_HEADER(L);
  int n = luaL_optinteger(L, 2, 0);

  if (n > hdr->nrecs){
    lua_pushinteger(L, 0);
    return 1;
  } else if ( n < -hdr->nrecs) {
    n = 0;
  } else if (n < 0) {
    n = hdr->nrecs + n;
  }

  hdr->reader = hdr->tail;
  lua_pushinteger(L, hdr->nrecs - n);

  while (n--) {
    bump(hdr, &hdr->reader);
  }
  DUMP_HEADER(hdr,fnd);
  
  return 1;
}

// Lua: s = buffer:readnext()
// Return the next readable record or nil if none are readable
static int history_readnext( lua_State* L ) {
  RETRIEVE_HEADER(L);

  if (hdr->reader == INVALID || hdr->reader == hdr->head) {
    hdr->reader = INVALID;
    return 0;
  }

  uint16 rec = hdr->reader;
  uint16 len = bump(hdr, &hdr->reader);
  rec += (len<128) ? 1 : 2;
  WRAP(rec);

  if (hdr->reader > rec) {
    lua_pushlstring (L, (char *)hdr->header + (sizeof(hdr->header)) + rec, len);
  } else {
    byte *p = luaM_malloc(L, len);
    memcpy(p, hdr->header + sizeof(hdr->header) + rec, hdr->size - rec);
    memcpy(p + hdr->size - rec, hdr->header + sizeof(hdr->header),  len - (hdr->size - rec));
    lua_pushlstring (L, (char *) p, len);
    luaM_free(L, p);
  }

  DUMP_HEADER(hdr,rdn);
  return 1;
}

// Module function map
static const LUA_REG_TYPE buffer_map[] = {
  { LSTRKEY("add"	),       LFUNCVAL( history_add	) },
  { LSTRKEY("find"	),     LFUNCVAL( history_find ) },
  { LSTRKEY("readnext"	), LFUNCVAL( history_readnext	) },
  { LSTRKEY( "__index" ),  LROVAL( buffer_map ) },
  { LNILKEY, LNILVAL }
};
static const LUA_REG_TYPE history_map[] = {
  { LSTRKEY("buffer"),     LFUNCVAL( history_buffer ) },
  { LNILKEY, LNILVAL }
};

int luaopen_history( lua_State *L ) {
  luaL_rometatable(L, BUFFER_METATABLE, (void *)buffer_map); // Metatable for history.buffer objects
  return 0;
}

NODEMCU_MODULE(HISTORY, "history", history_map, luaopen_history); 
