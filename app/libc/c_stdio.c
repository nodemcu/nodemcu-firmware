#include "c_stdio.h"
// #include "driver/uart.h"

int c_stdin = 999;
int c_stdout = 1000;
int c_stderr = 1001;

// FILE *c_fopen(const char *_name, const char *_type){
// }
// FILE *c_freopen(const char *_name, const char *_type, FILE *_f){
// }
// FILE *c_tmpfile(void){
// }

// int    c_putchar(int c){
// }

// int    c_printf(const char *c, ...){
// }

// int c_sprintf(char *c, const char *s, ...){
// }

// int    c_fprintf(FILE *f, const char *s, ...){
// }
// int    c_fscanf(FILE *f, const char *s, ...){
// }
// int    c_fclose(FILE *f){
// }
// int    c_fflush(FILE *f){
// }
// int    c_setvbuf(FILE *f, char *c, int d, size_t t){
// }
// void c_clearerr(FILE *f){
// }
// int    c_fseek(FILE *f, long l, int d){
// }
// long c_ftell( FILE *f){
// }
// int    c_fputs(const char *c, FILE *f){
// }
// char *c_fgets(char *c, int d, FILE *f){
// }
// int    c_ungetc(int d, FILE *f){
// }
// size_t c_fread(void *p, size_t _size, size_t _n, FILE *f){
// }
// size_t c_fwrite(const void *p, size_t _size, size_t _n, FILE *f){
// }
// int    c_feof(FILE *f){
// }
// int    c_ferror(FILE *f){
// }
// int    c_getc(FILE *f){
// }

#if defined( LUA_NUMBER_INTEGRAL )

#else

#define FLOATINGPT 1
#define NEWFP 1
#define ENDIAN_LITTLE 1234
#define ENDIAN_BIG  4321
#define ENDIAN_PDP  3412
#define ENDIAN ENDIAN_LITTLE

/* $Id: strichr.c,v 1.1.1.1 2006/08/23 17:03:06 pefo Exp $ */

/*
 * Copyright (c) 2000-2002 Opsycon AB  (www.opsycon.se)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
//#include <string.h>
#include "c_string.h"

char *
strichr(char *p, int c)
{
    char *t;

    if (p != NULL) {
        for(t = p; *t; t++);
        for (; t >= p; t--) {
            *(t + 1) = *t;
        }
        *p = c;
    }
    return (p);
}

/* $Id: str_fmt.c,v 1.1.1.1 2006/08/23 17:03:06 pefo Exp $ */

/*
 * Copyright (c) 2000-2002 Opsycon AB  (www.opsycon.se)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
//#include <string.h>
#include "c_string.h"

#define FMT_RJUST 0
#define FMT_LJUST 1
#define FMT_RJUST0 2
#define FMT_CENTER 3

/*
 *  Format string by inserting blanks.
 */

void
str_fmt(char *p, int size, int fmt)
{
    int n, m, len;

    len = strlen (p);
    switch (fmt) {
    case FMT_RJUST:
        for (n = size - len; n > 0; n--)
            strichr (p, ' ');
        break;
    case FMT_LJUST:
        for (m = size - len; m > 0; m--)
            strcat (p, " ");
        break;
    case FMT_RJUST0:
        for (n = size - len; n > 0; n--)
            strichr (p, '0');
        break;
    case FMT_CENTER:
        m = (size - len) / 2;
        n = size - (len + m);
        for (; m > 0; m--)
            strcat (p, " ");
        for (; n > 0; n--)
            strichr (p, ' ');
        break;
    }
}

/* $Id: strtoupp.c,v 1.1.1.1 2006/08/23 17:03:06 pefo Exp $ */

/*
 * Copyright (c) 2000-2002 Opsycon AB  (www.opsycon.se)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
//#include <string.h>
//#include <ctype.h>
#include "c_string.h"
#include "c_ctype.h"

void
strtoupper(char *p)
{
    if(!p)
        return;
    for (; *p; p++)
        *p = toupper (*p);
}

/* $Id: atob.c,v 1.1.1.1 2006/08/23 17:03:06 pefo Exp $ */

/*
 * Copyright (c) 2000-2002 Opsycon AB  (www.opsycon.se)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

//#include <sys/types.h>
//#include <string.h>
//#include <pmon.h>
#include "c_string.h"
typedef unsigned int u_int32_t;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef int32_t register_t;
typedef long long quad_t;
typedef unsigned long long u_quad_t;
typedef double rtype;

#ifndef __P
#define __P(args) args
#endif

static char * _getbase __P((char *, int *));
static int _atob __P((unsigned long long *, char *p, int));

static char *
_getbase(char *p, int *basep)
{
    if (p[0] == '0') {
        switch (p[1]) {
        case 'x':
            *basep = 16;
            break;
        case 't': case 'n':
            *basep = 10;
            break;
        case 'o':
            *basep = 8;
            break;
        default:
            *basep = 10;
            return (p);
        }
        return (p + 2);
    }
    *basep = 10;
    return (p);
}


/*
 *  _atob(vp,p,base)
 */
static int
_atob (u_quad_t *vp, char *p, int base)
{
    u_quad_t value, v1, v2;
    char *q, tmp[20];
    int digit;

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }

    if (base == 16 && (q = strchr (p, '.')) != 0) {
        if (q - p > sizeof(tmp) - 1)
            return (0);

        strncpy (tmp, p, q - p);
        tmp[q - p] = '\0';
        if (!_atob (&v1, tmp, 16))
            return (0);

        q++;
        if (strchr (q, '.'))
            return (0);

        if (!_atob (&v2, q, 16))
            return (0);
        *vp = (v1 << 16) + v2;
        return (1);
    }

    value = *vp = 0;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9')
            digit = *p - '0';
        else if (*p >= 'a' && *p <= 'f')
            digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F')
            digit = *p - 'A' + 10;
        else
            return (0);

        if (digit >= base)
            return (0);
        value *= base;
        value += digit;
    }
    *vp = value;
    return (1);
}

/*
 *  atob(vp,p,base) 
 *      converts p to binary result in vp, rtn 1 on success
 */
int
atob(u_int32_t *vp, char *p, int base)
{
    u_quad_t v;

    if (base == 0)
        p = _getbase (p, &base);
    if (_atob (&v, p, base)) {
        *vp = v;
        return (1);
    }
    return (0);
}


/*
 *  llatob(vp,p,base) 
 *      converts p to binary result in vp, rtn 1 on success
 */
int
llatob(u_quad_t *vp, char *p, int base)
{
    if (base == 0)
        p = _getbase (p, &base);
    return _atob(vp, p, base);
}


/*
 *  char *btoa(dst,value,base) 
 *      converts value to ascii, result in dst
 */
char *
btoa(char *dst, u_int value, int base)
{
    char buf[34], digit;
    int i, j, rem, neg;

    if (value == 0) {
        dst[0] = '0';
        dst[1] = 0;
        return (dst);
    }

    neg = 0;
    if (base == -10) {
        base = 10;
        if (value & (1L << 31)) {
            value = (~value) + 1;
            neg = 1;
        }
    }

    for (i = 0; value != 0; i++) {
        rem = value % base;
        value /= base;
        if (rem >= 0 && rem <= 9)
            digit = rem + '0';
        else if (rem >= 10 && rem <= 36)
            digit = (rem - 10) + 'a';
        buf[i] = digit;
    }

    buf[i] = 0;
    if (neg)
        strcat (buf, "-");

    /* reverse the string */
    for (i = 0, j = strlen (buf) - 1; j >= 0; i++, j--)
        dst[i] = buf[j];
    dst[i] = 0;
    return (dst);
}

/*
 *  char *btoa(dst,value,base) 
 *      converts value to ascii, result in dst
 */
char *
llbtoa(char *dst, u_quad_t value, int base)
{
    char buf[66], digit;
    int i, j, rem, neg;

    if (value == 0) {
        dst[0] = '0';
        dst[1] = 0;
        return (dst);
    }

    neg = 0;
    if (base == -10) {
        base = 10;
        if (value & (1LL << 63)) {
            value = (~value) + 1;
            neg = 1;
        }
    }

    for (i = 0; value != 0; i++) {
        rem = value % base;
        value /= base;
        if (rem >= 0 && rem <= 9)
            digit = rem + '0';
        else if (rem >= 10 && rem <= 36)
            digit = (rem - 10) + 'a';
        buf[i] = digit;
    }

    buf[i] = 0;
    if (neg)
        strcat (buf, "-");

    /* reverse the string */
    for (i = 0, j = strlen (buf) - 1; j >= 0; i++, j--)
        dst[i] = buf[j];
    dst[i] = 0;
    return (dst);
}

/*
 *  gethex(vp,p,n) 
 *      convert n hex digits from p to binary, result in vp, 
 *      rtn 1 on success
 */
int
gethex(int32_t *vp, char *p, int n)
{
    u_long v;
    int digit;

    for (v = 0; n > 0; n--) {
        if (*p == 0)
            return (0);
        if (*p >= '0' && *p <= '9')
            digit = *p - '0';
        else if (*p >= 'a' && *p <= 'f')
            digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F')
            digit = *p - 'A' + 10;
        else
            return (0);

        v <<= 4;
        v |= digit;
        p++;
    }
    *vp = v;
    return (1);
}

/* $Id: vsprintf.c,v 1.1.1.1 2006/08/23 17:03:06 pefo Exp $ */

/*
 * Copyright (c) 2000-2002 Opsycon AB  (www.opsycon.se)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
//#include <stdio.h>
//#include <stdarg.h>
//#include <string.h>
//#include <ctype.h>
//#include <pmon.h>
#include "c_stdarg.h"
#include "c_string.h"
#include "c_ctype.h"

/*
 *  int vsprintf(d,s,ap)
 */
int 
vsprintf (char *d, const char *s, va_list ap)
{
    const char *t;
    char *p, *dst, tmp[40];
    unsigned int n;
    int fmt, trunc, haddot, width, base, longlong;
#ifdef FLOATINGPT
    double dbl;

#ifndef NEWFP
    EP ex;
#endif
#endif

    dst = d;
    for (; *s;) {
        if (*s == '%') {
            s++;
            fmt = FMT_RJUST;
            width = trunc = haddot = longlong = 0;
            for (; *s; s++) {
                if (strchr("bcdefgilopPrRsuxX%", *s))
                    break;
                else if (*s == '-')
                    fmt = FMT_LJUST;
                else if (*s == '0')
                    fmt = FMT_RJUST0;
                else if (*s == '~')
                    fmt = FMT_CENTER;
                else if (*s == '*') {
                    if (haddot)
                        trunc = va_arg(ap, int);
                    else
                        width = va_arg(ap, int);
                } else if (*s >= '1' && *s <= '9') {
                    for (t = s; isdigit(*s); s++);
                    strncpy(tmp, t, s - t);
                    tmp[s - t] = '\0';
                    atob(&n, tmp, 10);
                    if (haddot)
                        trunc = n;
                    else
                        width = n;
                    s--;
                } else if (*s == '.')
                    haddot = 1;
            }
            if (*s == '%') {
                *d++ = '%';
                *d = 0;
            } else if (*s == 's') {
                p = va_arg(ap, char *);

                if (p)
                    strcpy(d, p);
                else
                    strcpy(d, "(null)");
            } else if (*s == 'c') {
                n = va_arg (ap, int);
                *d = n;
                d[1] = 0;
            } else {
                if (*s == 'l') {
                    if (*++s == 'l') {
                        longlong = 1;
                        ++s;
                    }
                }
                if (strchr("bdiopPrRxXu", *s)) {
                    if (*s == 'd' || *s == 'i')
                        base = -10;
                    else if (*s == 'u')
                        base = 10;
                    else if (*s == 'x' || *s == 'X')
                        base = 16;
                    else if(*s == 'p' || *s == 'P') {
                        base = 16;
                        if (*s == 'p') {
                            *d++ = '0';
                            *d++ = 'x';
                        }
                        fmt = FMT_RJUST0;
                        if (sizeof(long) > 4) {
                            width = 16;
                            longlong = 1;
                        } else {
                            width = 8;
                        }
                    }
                    else if(*s == 'r' || *s == 'R') {
                        base = 16;
                        if (*s == 'r') {
                            *d++ = '0';
                            *d++ = 'x';
                        }
                        fmt = FMT_RJUST0;
                        if (sizeof(register_t) > 4) {
                            width = 16;
                            longlong = 1;
                        } else {
                            width = 8;
                        }
                    }
                    else if (*s == 'o')
                        base = 8;
                    else if (*s == 'b')
                        base = 2;
                    if (longlong)
                        llbtoa(d, va_arg (ap, quad_t),
                            base);
                    else
                        btoa(d, va_arg (ap, int), base);

                    if (*s == 'X')
                        strtoupper(d);
                }
#ifdef FLOATINGPT
                else if (strchr ("eEfgG", *s)) {
//static void dtoa (char *, double, int, int, int);
void dtoa (char *dbuf, rtype arg, int fmtch, int width, int prec);
                    dbl = va_arg(ap, double);
                    dtoa(d, dbl, *s, width, trunc);
                    trunc = 0;
                }
#endif
            }
            if (trunc)
                d[trunc] = 0;
            if (width)
                str_fmt (d, width, fmt);
            for (; *d; d++);
            s++;
        } else
            *d++ = *s++;
    }
    *d = 0;
    return (d - dst);
} 

#ifdef FLOATINGPT
/*
 * Floating point output, cvt() onward lifted from BSD sources:
 *
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#define MAX_FCONVERSION 512 /* largest possible real conversion     */
#define MAX_EXPT    5   /* largest possible exponent field */
#define MAX_FRACT   39  /* largest possible fraction field */

#define TESTFLAG(x) 0


typedef double rtype;

extern double modf(double, double *);
#define to_char(n)  ((n) + '0')
#define to_digit(c) ((c) - '0')
#define _isNan(arg) ((arg) != (arg))

static int cvt (rtype arg, int prec, char *signp, int fmtch,
        char *startp, char *endp);
static char *c_round (double fract, int *exp, char *start, char *end, 
            char ch, char *signp);
static char *exponent(char *p, int exp, int fmtch);


/*
 * _finite arg not Infinity or Nan
 */
static int _finite(rtype d)
{
#if ENDIAN == ENDIAN_LITTLE
    struct IEEEdp {
    unsigned manl:32;
    unsigned manh:20;
    unsigned exp:11;
    unsigned sign:1;
    } *ip;
#else
    struct IEEEdp {
    unsigned sign:1;
    unsigned exp:11;
    unsigned manh:20;
    unsigned manl:32;
    } *ip;
#endif

    ip = (struct IEEEdp *)&d;
    return (ip->exp != 0x7ff);
}


void dtoa (char *dbuf, rtype arg, int fmtch, int width, int prec)
{
    char    buf[MAX_FCONVERSION+1], *cp;
    char    sign;
    int size;

    if( !_finite(arg) ) {
        if( _isNan(arg) )
            strcpy (dbuf, "NaN");
        else if( arg < 0) 
            strcpy (dbuf, "-Infinity");
        else
            strcpy (dbuf, "Infinity");
        return;
    }

    if (prec == 0)
        prec = 6;
    else if (prec > MAX_FRACT)
        prec = MAX_FRACT;

    /* leave room for sign at start of buffer */
    cp = buf + 1;

    /*
     * cvt may have to round up before the "start" of
     * its buffer, i.e. ``intf("%.2f", (double)9.999);'';
     * if the first character is still NUL, it did.
     * softsign avoids negative 0 if _double < 0 but
     * no significant digits will be shown.
     */
    *cp = '\0';
    size = cvt (arg, prec, &sign, fmtch, cp, buf + sizeof(buf));
    if (*cp == '\0')
        cp++;

    if (sign)
        *--cp = sign, size++;

    cp[size] = 0;
    memcpy (dbuf, cp, size + 1);
}


static int
cvt(rtype number, int prec, char *signp, int fmtch, char *startp, char *endp)
{
    register char *p, *t;
    register double fract;
    double integer, tmp;
    int dotrim, expcnt, gformat;

    dotrim = expcnt = gformat = 0;
    if (number < 0) {
        number = -number;
        *signp = '-';
    } else
        *signp = 0;

    fract = modf(number, &integer);

    /* get an extra slot for rounding. */
    t = ++startp;

    /*
     * get integer portion of number; put into the end of the buffer; the
     * .01 is added for modf(356.0 / 10, &integer) returning .59999999...
     */
    for (p = endp - 1; integer; ++expcnt) {
        tmp = modf(integer / 10, &integer);
        *p-- = to_char((int)((tmp + .01) * 10));
    }
    switch (fmtch) {
    case 'f':
        /* reverse integer into beginning of buffer */
        if (expcnt)
            for (; ++p < endp; *t++ = *p);
        else
            *t++ = '0';
        /*
         * if precision required or alternate flag set, add in a
         * decimal point.
         */
        if (prec || TESTFLAG(ALTERNATE_FORM))
            *t++ = '.';
        /* if requires more precision and some fraction left */
        if (fract) {
            if (prec)
                do {
                    fract = modf(fract * 10, &tmp);
                    *t++ = to_char((int)tmp);
                } while (--prec && fract);
            if (fract)
                startp = c_round(fract, (int *)NULL, startp,
                    t - 1, (char)0, signp);
        }
        for (; prec--; *t++ = '0');
        break;
    case 'e':
    case 'E':
eformat:    if (expcnt) {
            *t++ = *++p;
            if (prec || TESTFLAG(ALTERNATE_FORM))
                *t++ = '.';
            /* if requires more precision and some integer left */
            for (; prec && ++p < endp; --prec)
                *t++ = *p;
            /*
             * if done precision and more of the integer component,
             * round using it; adjust fract so we don't re-round
             * later.
             */
            if (!prec && ++p < endp) {
                fract = 0;
                startp = c_round((double)0, &expcnt, startp,
                    t - 1, *p, signp);
            }
            /* adjust expcnt for digit in front of decimal */
            --expcnt;
        }
        /* until first fractional digit, decrement exponent */
        else if (fract) {
            /* adjust expcnt for digit in front of decimal */
            for (expcnt = -1;; --expcnt) {
                fract = modf(fract * 10, &tmp);
                if (tmp)
                    break;
            }
            *t++ = to_char((int)tmp);
            if (prec || TESTFLAG(ALTERNATE_FORM))
                *t++ = '.';
        }
        else {
            *t++ = '0';
            if (prec || TESTFLAG(ALTERNATE_FORM))
                *t++ = '.';
        }
        /* if requires more precision and some fraction left */
        if (fract) {
            if (prec)
                do {
                    fract = modf(fract * 10, &tmp);
                    *t++ = to_char((int)tmp);
                } while (--prec && fract);
            if (fract)
                startp = c_round(fract, &expcnt, startp,
                    t - 1, (char)0, signp);
        }
        /* if requires more precision */
        for (; prec--; *t++ = '0');

        /* unless alternate flag, trim any g/G format trailing 0's */
        if (gformat && !TESTFLAG(ALTERNATE_FORM)) {
            while (t > startp && *--t == '0');
            if (*t == '.')
                --t;
            ++t;
        }
        t = exponent(t, expcnt, fmtch);
        break;
    case 'g':
    case 'G':
        /* a precision of 0 is treated as a precision of 1. */
        if (!prec)
            ++prec;
        /*
         * ``The style used depends on the value converted; style e
         * will be used only if the exponent resulting from the
         * conversion is less than -4 or greater than the precision.''
         *  -- ANSI X3J11
         */
        if (expcnt > prec || (!expcnt && fract && fract < .0001)) {
            /*
             * g/G format counts "significant digits, not digits of
             * precision; for the e/E format, this just causes an
             * off-by-one problem, i.e. g/G considers the digit
             * before the decimal point significant and e/E doesn't
             * count it as precision.
             */
            --prec;
            fmtch -= 2;     /* G->E, g->e */
            gformat = 1;
            goto eformat;
        }
        /*
         * reverse integer into beginning of buffer,
         * note, decrement precision
         */
        if (expcnt)
            for (; ++p < endp; *t++ = *p, --prec);
        else
            *t++ = '0';
        /*
         * if precision required or alternate flag set, add in a
         * decimal point.  If no digits yet, add in leading 0.
         */
        if (prec || TESTFLAG(ALTERNATE_FORM)) {
            dotrim = 1;
            *t++ = '.';
        }
        else
            dotrim = 0;
        /* if requires more precision and some fraction left */
        if (fract) {
            if (prec) {
                    do {
                    fract = modf(fract * 10, &tmp);
                    *t++ = to_char((int)tmp);
                } while(!tmp && !expcnt);
                while (--prec && fract) {
                    fract = modf(fract * 10, &tmp);
                    *t++ = to_char((int)tmp);
                }
            }
            if (fract)
                startp = c_round(fract, (int *)NULL, startp,
                    t - 1, (char)0, signp);
        }
        /* alternate format, adds 0's for precision, else trim 0's */
        if (TESTFLAG(ALTERNATE_FORM))
            for (; prec--; *t++ = '0');
        else if (dotrim) {
            while (t > startp && *--t == '0');
            if (*t != '.')
                ++t;
        }
    }
    return (t - startp);
}


static char *
c_round(double fract, int *exp, char *start, char *end, char ch, char *signp)
{
    double tmp;

    if (fract)
        (void)modf(fract * 10, &tmp);
    else
        tmp = to_digit(ch);
    if (tmp > 4)
        for (;; --end) {
            if (*end == '.')
                --end;
            if (++*end <= '9')
                break;
            *end = '0';
            if (end == start) {
                if (exp) {  /* e/E; increment exponent */
                    *end = '1';
                    ++*exp;
                }
                else {      /* f; add extra digit */
                *--end = '1';
                --start;
                }
                break;
            }
        }
    /* ``"%.3f", (double)-0.0004'' gives you a negative 0. */
    else if (*signp == '-')
        for (;; --end) {
            if (*end == '.')
                --end;
            if (*end != '0')
                break;
            if (end == start)
                *signp = 0;
        }
    return (start);
}

static char *
exponent(char *p, int exp, int fmtch)
{
    char *t;
    char expbuf[MAX_FCONVERSION];

    *p++ = fmtch;
    if (exp < 0) {
        exp = -exp;
        *p++ = '-';
    }
    else
        *p++ = '+';
    t = expbuf + MAX_FCONVERSION;
    if (exp > 9) {
        do {
            *--t = to_char(exp % 10);
        } while ((exp /= 10) > 9);
        *--t = to_char(exp);
        for (; t < expbuf + MAX_FCONVERSION; *p++ = *t++);
    }
    else {
        *p++ = '0';
        *p++ = to_char(exp);
    }
    return (p);
}
#endif /* FLOATINGPT */


int c_sprintf(char *s, const char *fmt, ...)
{
    int n;
    va_list arg;
    va_start(arg, fmt);
    n = vsprintf(s, fmt, arg);
    va_end(arg);
    return n;
}

#endif
