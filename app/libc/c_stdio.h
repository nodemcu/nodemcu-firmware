#ifndef _C_STDIO_H_
#define _C_STDIO_H_

#define __need_size_t

#include "c_stddef.h"
#include "osapi.h"
// #include "driver/uart.h"

// #define __need___va_list
//#include "c_stdarg.h"

//struct __sFILE{
//  int	_r;		/* read space left for getc() */
//  int	_w;		/* write space left for putc() */
//};
// typedef struct __sFILE   __FILE;
// typedef __FILE FILE;

extern int c_stdin;
extern int c_stdout;
extern int c_stderr;

// #define	_IOFBF	0		/* setvbuf should set fully buffered */
// #define	_IOLBF	1		/* setvbuf should set line buffered */
// #define	_IONBF	2		/* setvbuf should set unbuffered */

// #ifndef NULL
// #define	NULL	0
// #endif

#define	EOF	(-1)

#ifdef __BUFSIZ__
#define   BUFSIZ         __BUFSIZ__
#else
#define   BUFSIZ         1024
#endif

#ifndef SEEK_SET
#define	SEEK_SET	0	/* set file offset to offset */
#endif
#ifndef SEEK_CUR
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#endif
#ifndef SEEK_END
#define	SEEK_END	2	/* set file offset to EOF plus offset */
#endif

// #define c_malloc os_malloc
// #define c_zalloc os_zalloc
// #define c_free os_free

extern void output_redirect(const char *str);
#define c_puts output_redirect

// #define c_printf os_printf
// int	c_printf(const char *c, ...);
#if defined( LUA_NUMBER_INTEGRAL )
#define c_sprintf os_sprintf
#else
#include "c_stdarg.h"
int c_sprintf(char* s,const char *fmt, ...);
#endif

extern void dbg_printf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

#define c_vsprintf ets_vsprintf
#define c_printf(...) do {					\
	unsigned char __print_buf[BUFSIZ];		\
	c_sprintf(__print_buf, __VA_ARGS__);	\
	c_puts(__print_buf);					\
} while(0)

// #define c_getc ets_getc
// #define c_getchar ets_getc
// note: contact esp to ensure the real getchar function..

// FILE *c_fopen(const char *_name, const char *_type);
// FILE *c_freopen(const char *, const char *, FILE *);
// FILE *c_tmpfile(void);

// int	c_putchar(int);
// int	c_printf(const char *, ...);
// int  c_sprintf(char *, const char *, ...);
// int	c_getc(FILE *);

// int	c_ungetc(int, FILE *);

// int	c_fprintf(FILE *, const char *, ...);
// int	c_fscanf(FILE *, const char *, ...);
// int	c_fclose(FILE *);
// int	c_fflush(FILE *);
// int	c_setvbuf(FILE *, char *, int, size_t);
// void c_clearerr(FILE *);
// int	c_fseek(FILE *, long, int);
// long c_ftell( FILE *);
// int	c_fputs(const char *, FILE *);
// char *c_fgets(char *, int, FILE *);
// size_t c_fread(void *, size_t _size, size_t _n, FILE *);
// size_t c_fwrite(const void * , size_t _size, size_t _n, FILE *);
// int	c_feof(FILE *);
// int	c_ferror(FILE *);

#endif /* _C_STDIO_H_ */
