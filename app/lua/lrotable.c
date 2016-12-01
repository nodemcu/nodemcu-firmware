/* Read-only tables for Lua */
#define LUAC_CROSS_FILE

#include "lua.h"
#include C_HEADER_STRING
#include "lrotable.h"
#include "lauxlib.h"
#include "lstring.h"
#include "lobject.h"
#include "lapi.h"
#include "memeq.h"

/* Local defines */
#define LUAR_FINDFUNCTION     0
#define LUAR_FINDVALUE        1

#undef NODE_DBG
#define NODE_DBG dbg_printf

/* Externally defined read-only table array */
extern const luaR_table lua_rotable[];
const uint32_t __attribute__((weak)) lua_rotable_table_hashtable[1];

const uint8_t __attribute__((section(".keep.rodata"))) luaR_table_size = sizeof(luaR_table);
const uint8_t __attribute__((section(".keep.rodata"))) luaR_entry_size = sizeof(luaR_entry);

static uint32_t strhash(const char *str) {
  uint32_t l = c_strlen(str);
  unsigned int h = cast(unsigned int, l);  /* seed */
  size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */
  size_t l1;
  for (l1=l; l1>=step; l1-=step)  /* compute hash */
    h = h ^ ((h<<5)+(h>>2)+cast(unsigned char, str[l1-1]));

  return h;
}

/* Find a global "read only table" in the constant lua_rotable array */
luaR_table* luaR_findglobalhash(const char *name, uint32_t hash) {
  unsigned i;

  const uint32_t *bits = lua_rotable_table_hashtable;
  uint32_t mask = *bits++;
  if (mask) {
    int probeval;
    for (int probe = hash & mask; probeval = (bits[probe >> 2] >> ((probe & 3) << 3)) & 0xff; probe = (probe + 1) & mask) {
      const luaR_table *pthis = lua_rotable + (probeval & 0x7f) - 1;
      if (!memeq(pthis->name, name, pthis->namesize)) {
        return (luaR_table *) pthis;
      }
      if (probeval & 0x80) {
        break;
      }
    }

    return NULL;
  }

  NODE_DBG("rotable is not optimized\n");

  for (i=0; lua_rotable[i].name; i ++) {
    if (!c_strcmp(lua_rotable[i].name, name)) {
      return (luaR_table *) &(lua_rotable[i]);
    }
  }
  return NULL;
}

/* Find an entry in a rotable and return it */
static const TValue* luaR_auxfind(const luaR_table *ptable, const char *strkey, luaR_numkey numkey, unsigned *ppos) {
  const luaR_entry *pentry = ptable->pentries;
  const TValue *res = NULL;
  unsigned i = 0;
  
  if (pentry == NULL)
    return NULL;  
  NODE_DBG("Looking up %s/%d in %s\n", strkey, numkey, ptable->name);
  while(pentry->key.type != LUA_TNIL) {
    if ((strkey && ((pentry->key.type & 0xff) == LUA_TSTRING) && (!c_strcmp(pentry->key.id.strkey, strkey))) || 
        (!strkey && (pentry->key.type == LUA_TNUMBER) && ((luaR_numkey)pentry->key.id.numkey == numkey))) {
      res = &pentry->value;
      break;
    }
    i ++; pentry ++;
  }
  if (res && ppos)
    *ppos = i;   
  return res;
}

static const TValue* luaR_auxfindstr(const luaR_table *ptable, const char *strkey, uint32_t hash) {
  const luaR_entry *pentry = ptable->pentries;

  if (pentry == NULL)
    return NULL;  
  
  const uint32_t *bits = ptable->hashtable;
  if (bits) {
    if (hash == 0) {
      hash = strhash(strkey);
      NODE_DBG("Hashing %s to 0x%x\n", strkey, hash);
    }
  
    uint32_t mask = *bits++;
    if (mask) {
      int probeval;
      for (int probe = hash & mask; probeval = (bits[probe >> 2] >> ((probe & 3) << 3)) & 0xff; probe = (probe + 1) & mask) {
        const luaR_entry *pthis = pentry + (probeval & 0x7f) - 1;
        if (!memeq(pthis->key.id.strkey, strkey, pthis->key.type >> 8)) {
          return &pthis->value;
        }
        if (probeval & 0x80) {
          break;
        }
      }

      return NULL;
    }
  }

  NODE_DBG("Looking up %s in %s (nonopt) bits=%x, tbl=%x\n", strkey, ptable->name, (uint32_t) bits, (uint32_t) ptable);

  while (pentry->key.type != LUA_TNIL) {
    if (((pentry->key.type & 0xff) == LUA_TSTRING) && (!c_strcmp(pentry->key.id.strkey, strkey))) {
      return &pentry->value;
    }
    pentry ++;
  }
  return NULL;
}

int luaR_findfunction(lua_State *L, const luaR_table *ptable) {
  const TValue *res = NULL;
  const char *key = luaL_checkstring(L, 2);
  uint32_t hash = tsvalue(L->base + 2 - 1)->hash;
    
  res = luaR_auxfindstr(ptable, key, hash);  
  if (res && ttislightfunction(res)) {
    luaA_pushobject(L, res);
    return 1;
  }
  else
    return 0;
}

/* Find an entry in a rotable and return its type 
   If "strkey" is not NULL, the function will look for a string key,
   otherwise it will look for a number key */
const TValue* luaR_findentry(const luaR_table *ptable, const char *strkey, luaR_numkey numkey, unsigned *ppos) {
  return luaR_auxfind(ptable, strkey, numkey, ppos);
}

const TValue* luaR_findentrytstr(const luaR_table *ptable, const TString *key) {
  return luaR_auxfindstr(ptable, getstr(key), key->tsv.hash);
}

/* Find the metatable of a given table */
void* luaR_getmeta(const luaR_table *ptable) {
#ifdef LUA_META_ROTABLES
  const TValue *res = luaR_auxfindstr(ptable, "__metatable", 0xbdd03a15);
  return res && ttisrotable(res) ? rvalue(res) : NULL;
#else
  return NULL;
#endif
}

static void luaR_next_helper(lua_State *L, const luaR_entry *pentries, int pos, TValue *key, TValue *val) {
  setnilvalue(key);
  setnilvalue(val);
  if (pentries[pos].key.type != LUA_TNIL) {
    /* Found an entry */
    if ((pentries[pos].key.type  & 0xff) == LUA_TSTRING)
      setsvalue(L, key, luaS_newro(L, pentries[pos].key.id.strkey))
    else
      setnvalue(key, (lua_Number)pentries[pos].key.id.numkey)
   setobj2s(L, val, &pentries[pos].value);
  }
}
/* next (used for iteration) */
void luaR_next(lua_State *L, const luaR_table *ptable, TValue *key, TValue *val) {
  const luaR_entry* pentries = ptable->pentries;
  char strkey[LUA_MAX_ROTABLE_NAME + 1], *pstrkey = NULL;
  luaR_numkey numkey = 0;
  unsigned keypos;
  
  /* Special case: if key is nil, return the first element of the rotable */
  if (ttisnil(key)) 
    luaR_next_helper(L, pentries, 0, key, val);
  else if (ttisstring(key) || ttisnumber(key)) {
    /* Find the previoud key again */  
    if (ttisstring(key)) {
      luaR_getcstr(strkey, rawtsvalue(key), LUA_MAX_ROTABLE_NAME);          
      pstrkey = strkey;
    } else   
      numkey = (luaR_numkey)nvalue(key);
    luaR_findentry(ptable, pstrkey, numkey, &keypos);
    /* Advance to next key */
    keypos ++;    
    luaR_next_helper(L, pentries, keypos, key, val);
  }
}

/* Convert a Lua string to a C string */
void luaR_getcstr(char *dest, const TString *src, size_t maxsize) {
  if (src->tsv.len+1 > maxsize)
    dest[0] = '\0';
  else {
    c_memcpy(dest, getstr(src), src->tsv.len);
    dest[src->tsv.len] = '\0';
  } 
}

/* Return 1 if the given pointer is a rotable */
#ifdef LUA_META_ROTABLES

#include "compiler.h"

int luaR_isrotable(void *p) {
  return RODATA_START_ADDRESS <= (char*)p && (char*)p <= RODATA_END_ADDRESS;
}
#endif
