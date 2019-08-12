/*
** Header to allow luac.cross compile within NodeMCU
** See Copyright Notice in lua.h
*/
#ifndef luac_cross_h
#define luac_cross_h

#ifdef LUA_CROSS_COMPILER
#define ICACHE_RODATA_ATTR

#define c_stderr stderr
#define c_stdin stdin
#define c_stdout stdout

#define dbg_printf printf
#else

#endif /* LUA_CROSS_COMPILER */
#endif /* luac_cross_h */
