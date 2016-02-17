/*
 * app/modules/cirbuf.c
 *
 * Copyright (c) 2016 Henk Vergonet <Henk.Vergonet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "lua.h"
#include "lauxlib.h"
#include "module.h"
#include "c_string.h"

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HTOLE(a)	(a)
#else /* BIG_ENDIAN */
#define HTOLE(a)					 	\
	(sizeof(lua_Integer) == 2 ? htole16(a) : 		\
	 (sizeof(lua_Integer) == 8 ? htole64(a) : htole32(a) ) )
#endif /* BIG_ENDIAN */

struct cirbuf_h {
	size_t	elsiz;
	uint8_t *tail, *pos, *end;
	int	rol;
	uint8_t data[4];
};

/**
  * handle = cirbuf.new(elnum, elsize)
  *
  * Creates a new circular buffer returns at maximum <elnum> elements,
  * of size <elsize>.
  */
static int cirbuf_new( lua_State* L )
{
  struct cirbuf_h *buf;
  size_t elnum, elsiz, len;

  if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
	return luaL_error(L, "wrong arg range");

  elnum = luaL_checkinteger(L, 1) + 1;
  elsiz = luaL_checkinteger(L, 2);
  if (elsiz == 0 || elnum <= 2)
	return luaL_error(L, "wrong arg range");

  len = sizeof(*buf) - sizeof(buf->data) + elnum*elsiz;
  buf = (struct cirbuf_h *)lua_newuserdata(L, len);
  /* lua_newuserdata will trow an exception when memory cant be allocated */

  c_memset(buf, 0, len);
  buf->elsiz = elsiz;
  buf->tail  = buf->data;
  buf->end   = &buf->data[elnum*elsiz];
  return 1;
}

/**
  * cirbuf.push(handle, data)
  *
  * Add a new data point to the buffer.
  */
static int cirbuf_push( lua_State* L )
{
  struct cirbuf_h *buf = (struct cirbuf_h *)lua_touserdata(L, 1);
  const char 	*pval;
  lua_Integer	val;
  size_t	len;

  luaL_argcheck(L, buf != NULL, 1, "`cirbuf' expected");

  // mutex
  size_t  elsiz = buf->elsiz;
  uint8_t *tail = buf->tail;
  int     ix = 2;
  do {
     if (lua_isnumber(L, ix)) {
  	val = HTOLE(luaL_checkinteger(L, ix));
	pval = (const char *)&val;
	len = sizeof(val);
     } else if (lua_isstring(L, ix)) {
  	pval = luaL_checklstring(L, ix, &len);
	// len++; // include the terminating 0
     } else {
     	break;
     }

     /* push value in buffer */ 
     if (len >= elsiz)
     	len = elsiz;
     c_memcpy(tail, pval, len);
     elsiz -= len;
     tail  += len;
     ix++;
  } while(elsiz);

  if(elsiz)
	c_memset(tail, 0, elsiz);

  buf->tail += buf->elsiz;
  if (buf->tail >= buf->end) {
  	buf->tail = buf->data;
	buf->rol  = 1;
  }
  // mutex
  return 0;
}

/**
  * buf = cirbuf.readstart(handle, [maxelem])
  *
  * Reads at beginning of the sample series,
  */
static int cirbuf_readstart( lua_State* L )
{
  struct cirbuf_h *buf = (struct cirbuf_h *)lua_touserdata(L, 1);
  size_t len;
  uint8_t *tmp;

  luaL_argcheck(L, buf != NULL, 1, "`cirbuf' expected");

  // mutex
  buf->pos = buf->data;
  len = buf->tail - buf->pos;
  tmp = buf->tail + buf->elsiz;
  if (buf->rol && tmp < buf->end) {
	buf->pos = tmp;
	len = buf->end - tmp;
  }
  if (lua_isnumber(L, 2)) {
	size_t maxlen = buf->elsiz * luaL_checkinteger(L, 2);
	if (maxlen > 0 && maxlen < len)
		len = maxlen;
  }
  lua_pushlstring(L, (char *)buf->pos, len);
  buf->pos += len;

  if (buf->pos >= buf->end)
	buf->pos = buf->data;
  // mutex
  return 1;
}

/**
  * buf = cirbuf.readnext(handle, [maxelem])
  *
  * Read next segment of buffer.
  */
static int cirbuf_readnext( lua_State* L )
{
  struct cirbuf_h *buf = (struct cirbuf_h *)lua_touserdata(L, 1);
  size_t len;

  luaL_argcheck(L, buf != NULL, 1, "`cirbuf' expected");

  // mutex
  len = buf->tail - buf->pos;
  if (!len) {
	// mutex
	lua_pushnil(L);
	return 1;
  }

  if (buf->pos > buf->tail)
	len = buf->end - buf->pos;
  if (lua_isnumber(L, 2)) {
	size_t maxlen = buf->elsiz * luaL_checkinteger(L, 2);
	if (maxlen > 0 && maxlen < len)
		len = maxlen;
  }

  lua_pushlstring(L, (char *)buf->pos, len);
  buf->pos += len;

  if (buf->pos >= buf->end)
	buf->pos = buf->data;
  // mutex
  return 1;
}

// Module function map
static const LUA_REG_TYPE cirbuf_map[] = {
  { LSTRKEY("new"	), LFUNCVAL( cirbuf_new		) },
  { LSTRKEY("push"	), LFUNCVAL( cirbuf_push	) },
  { LSTRKEY("readstart"	), LFUNCVAL( cirbuf_readstart	) },
  { LSTRKEY("readnext"	), LFUNCVAL( cirbuf_readnext	) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(CIRBUF, "cirbuf", cirbuf_map, NULL);

