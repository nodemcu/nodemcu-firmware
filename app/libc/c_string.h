/*
 * c_string.h
 *
 * Definitions for memory and string functions.
 */

#ifndef _C_STRING_H_
#define	_C_STRING_H_
#include "c_stddef.h"
#include "osapi.h"

#ifndef NULL
#define NULL 0
#endif

#define c_memcmp os_memcmp
#define c_memcpy os_memcpy
#define c_memset os_memset

#define c_strcat os_strcat
#define c_strchr os_strchr
#define c_strcmp os_strcmp
#define c_strcpy os_strcpy
#define c_strlen os_strlen
#define c_strncmp os_strncmp
#define c_strncpy os_strncpy
// #define c_strstr os_strstr
#define c_strncasecmp c_strncmp

#define c_strstr strstr
#define c_strncat strncat
#define c_strcspn strcspn
#define c_strpbrk strpbrk
#define c_strcoll strcoll
#define c_strrchr strrchr

// const char *c_strstr(const char * __s1, const char * __s2);
// char *c_strncat(char * __restrict /*s1*/, const char * __restrict /*s2*/, size_t n);
// size_t c_strcspn(const char * s1, const char * s2);
// const char *c_strpbrk(const char * /*s1*/, const char * /*s2*/);
// int c_strcoll(const char * /*s1*/, const char * /*s2*/);
//

extern size_t c_strlcpy(char *dst, const char *src, size_t siz);
extern size_t c_strlcat(char *dst, const char *src, size_t siz);
extern char *c_strdup(const char *src);


#endif /* _C_STRING_H_ */
