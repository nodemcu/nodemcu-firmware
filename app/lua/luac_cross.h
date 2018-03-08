/*
** Header to allow luac.cross compile within NodeMCU
** See Copyright Notice in lua.h
*/
#ifndef luac_cross_h
#define luac_cross_h

#ifdef LUA_CROSS_COMPILER

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

#define ICACHE_RODATA_ATTR

#define c_abs abs
#define c_exit exit
#define c_fclose fclose
#define c_feof feof
#define c_ferror ferror
#define c_fopen fopen
#define c_fread fread
#define c_free free
#define c_freopen freopen
#define c_getc getc
#define c_getenv getenv
#define c_malloc malloc
#define c_memcmp memcmp
#define c_memcpy memcpy
#define c_printf printf
#define c_puts puts
#define c_reader reader
#define c_realloc realloc
#define c_sprintf sprintf
#define c_stderr stderr
#define c_stdin stdin
#define c_stdout stdout
#define c_strcat strcat
#define c_strchr strchr
#define c_strcmp strcmp
#define c_strcoll strcoll
#define c_strcpy strcpy
#define c_strcspn strcspn
#define c_strerror strerror
#define c_strlen strlen
#define c_strncat strncat
#define c_strncmp strncmp
#define c_strncpy strncpy
#define c_strpbrk strpbrk
#define c_strrchr strrchr
#define c_strstr strstr
double	c_strtod(const char *__n, char **__end_PTR);
#define c_ungetc ungetc
#define c_strtol strtol
#define c_strtoul strtoul
#define dbg_printf printf
#else

#define C_HEADER_ASSERT "c_assert.h"
#define C_HEADER_CTYPE "c_ctype.h"
#define C_HEADER_ERRNO "c_errno.h"
#define C_HEADER_FCNTL "c_fcntl.h"
#define C_HEADER_LOCALE "c_locale.h"
#define C_HEADER_MATH "c_math.h"
#define C_HEADER_STDIO "c_stdio.h"
#define C_HEADER_STDLIB "c_stdlib.h"
#define C_HEADER_STRING "c_string.h"
#define C_HEADER_TIME "c_time.h"

#endif /* LUA_CROSS_COMPILER */
#endif /* luac_cross_h */
