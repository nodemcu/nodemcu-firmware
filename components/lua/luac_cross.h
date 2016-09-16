/*
** Header to allow luac.cross compile within NodeMCU
** See Copyright Notice in lua.h
*/
#ifndef luac_cross_h
#define luac_cross_h

#define C_HEADER_ASSERT <assert.h>
#define C_HEADER_CTYPE <ctype.h>
#define C_HEADER_ERRNO <errno.h>
#define C_HEADER_FCNTL <fcntl.h>
#define C_HEADER_LOCALE <locale.h>
#define C_HEADER_MATH <math.h>
#define C_HEADER_STDIO <stdio.h>
#define C_HEADER_STDLIB <stdlib.h>
#define C_HEADER_STRING <string.h>
#define C_HEADER_TIME <time.h>

#ifdef LUA_CROSS_COMPILER
#define ICACHE_RODATA_ATTR
#endif

#endif /* luac_cross_h */
