/*
 * c_stdlib.h
 *
 * Definitions for common types, variables, and functions.
 */

#ifndef _C_STDLIB_H_
#define _C_STDLIB_H_

#include "c_stddef.h"
#include "mem.h"

#include <stdlib.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define __INT_MAX__ 2147483647
#undef __RAND_MAX
#if __INT_MAX__ == 32767
#define __RAND_MAX 32767
#else
#define __RAND_MAX 0x7fffffff
#endif
#define RAND_MAX __RAND_MAX

#ifndef mem_realloc
#define mem_realloc pvPortRealloc
#endif
#ifndef os_realloc
#define os_realloc(p, s) mem_realloc((p), (s))
#endif

#define c_free os_free
#define c_malloc os_malloc
#define c_zalloc os_zalloc
#define c_realloc os_realloc

#define c_abs	abs
#define c_atoi	atoi
//#define c_strtod	strtod
#define c_strtol	strtol
#define c_strtoul	strtoul

// int c_abs(int);

// void c_exit(int);

//const char *c_getenv(const char *__string);

// void *c_malloc(size_t __size);
// void *c_zalloc(size_t __size);
// void c_free(void *);

// int	c_rand(void);
// void c_srand(unsigned int __seed);

// int	c_atoi(const char *__nptr);
double	c_strtod(const char *__n, char **__end_PTR);
// // long	c_strtol(const char *__n, char **__end_PTR, int __base);
// unsigned long c_strtoul(const char *__n, char **__end_PTR, int __base);
// // long long c_strtoll(const char *__n, char **__end_PTR, int __base);

#endif /* _C_STDLIB_H_ */
