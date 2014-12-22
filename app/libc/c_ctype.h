#ifndef _C_CTYPE_H_
#define _C_CTYPE_H_

#if 0
int isalnum(int);
int isalpha(int);
int iscntrl(int);
int isdigit(int);
// int isgraph(int);
int islower(int);
int isprint(int);
int ispunct(int);
int isspace(int);
int isupper(int);
int isxdigit(int);
int tolower(int);
int toupper(int);

#if !defined(__STRICT_ANSI__) || defined(__cplusplus) || __STDC_VERSION__ >= 199901L
// int isblank(int);
#endif

#ifndef __STRICT_ANSI__
// int isascii(int);
// int toascii(int);
#define _tolower(__c) ((unsigned char)(__c) - 'A' + 'a')
#define _toupper(__c) ((unsigned char)(__c) - 'a' + 'A')
#endif

#define	_U	01
#define	_L	02
#define	_N	04
#define	_S	010
#define _P	020
#define _C	040
#define _X	0100
#define	_B	0200

/* For C++ backward-compatibility only.  */
// extern	char	_ctype_[];
#endif
#endif /* _C_CTYPE_H_ */
